#include "external_import.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <filesystem>
#include <limits>
#include <map>
#include <sstream>
#include <stdexcept>
#include <utility>

#include "formats/mseed/modified_mseed_export.hpp"
#include "formats/tdms/tdms_export.hpp"
#include "qrest_data_hdf5/hdf5_reader.hpp"
#include "qrest_data_hdf5/hdf5_writer.hpp"
#include "qrest_file.hpp"

namespace qrest_data::tools {
namespace {

double unit_scale(TdmsImportOptions::Unit unit) {
    switch (unit) {
        case TdmsImportOptions::Unit::MeterPerSecondSquared:
            return 1.0;
        case TdmsImportOptions::Unit::CentimeterPerSecondSquared:
            return 100.0;
    }
    throw std::runtime_error("unsupported TDMS output unit");
}

tdms::SensitivitySelection
to_tdms_selection(TdmsImportOptions::SensitivitySelection selection) {
    switch (selection) {
        case TdmsImportOptions::SensitivitySelection::Acquisition:
            return tdms::SensitivitySelection::Acquisition;
        case TdmsImportOptions::SensitivitySelection::First:
            return tdms::SensitivitySelection::First;
        case TdmsImportOptions::SensitivitySelection::Last:
            return tdms::SensitivitySelection::Last;
        case TdmsImportOptions::SensitivitySelection::Explicit:
            return tdms::SensitivitySelection::Explicit;
    }
    throw std::runtime_error("unsupported TDMS sensitivity selection");
}

void add_error(ValidationReport &report, std::string message) {
    report.errors.push_back(std::move(message));
}

void add_warning(ValidationReport &report, std::string message) {
    report.warnings.push_back(std::move(message));
}

bool almost_equal(double a, double b, double rel = 1e-8) {
    const double scale = std::max({1.0, std::abs(a), std::abs(b)});
    return std::abs(a - b) <= rel * scale;
}

double metadata_sample_rate_hz(const Metadata &metadata) {
    if (!(metadata.DataInfo.DT > 0.0) || !std::isfinite(metadata.DataInfo.DT)) {
        return 0.0;
    }
    return 1.0 / metadata.DataInfo.DT;
}

void append_channel(std::vector<double> &dst,
                    const std::vector<std::int32_t> &counts,
                    double actual_sensitivity,
                    double output_unit_scale,
                    double post_scale,
                    bool output_counts) {
    dst.reserve(dst.size() + counts.size());
    for (const auto count : counts) {
        if (output_counts) {
            dst.push_back(static_cast<double>(count));
        } else {
            const double value_mps2 =
                static_cast<double>(count) / actual_sensitivity;
            dst.push_back(value_mps2 * output_unit_scale * post_scale);
        }
    }
}

std::string upper_ascii(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), [](char c) {
        return static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
    });
    return value;
}

std::string lower_ascii(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), [](char c) {
        return static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    });
    return value;
}

bool has_extension(const std::filesystem::path &path,
                   const std::vector<std::string> &extensions) {
    const auto ext = lower_ascii(path.extension().string());
    return std::find(extensions.begin(), extensions.end(), ext)
           != extensions.end();
}

std::vector<std::filesystem::path>
collect_input_files(const std::string &input_path,
                    const std::vector<std::string> &extensions,
                    const char *format_name) {
    const std::filesystem::path path(input_path);
    std::vector<std::filesystem::path> files;

    if (std::filesystem::is_regular_file(path)) {
        if (!has_extension(path, extensions)) {
            std::ostringstream oss;
            oss << format_name
                << " input file has unsupported extension: " << path.string();
            throw std::runtime_error(oss.str());
        }
        files.push_back(path);
    } else if (std::filesystem::is_directory(path)) {
        for (const auto &entry : std::filesystem::directory_iterator(path)) {
            if (entry.is_regular_file()
                && has_extension(entry.path(), extensions)) {
                files.push_back(entry.path());
            }
        }
        std::sort(
            files.begin(), files.end(), [](const auto &lhs, const auto &rhs) {
                return lhs.filename().string() < rhs.filename().string();
            });
    } else {
        throw std::runtime_error(std::string(format_name)
                                 + " input path is neither file nor directory: "
                                 + input_path);
    }

    if (files.empty()) {
        throw std::runtime_error(std::string("No ") + format_name
                                 + " files found in input path: " + input_path);
    }
    return files;
}

char canonical_direction(const std::string &label) {
    const auto upper = upper_ascii(label);
    if (upper == "E" || upper == "EIE" || upper == "X") {
        return 'X';
    }
    if (upper == "N" || upper == "EIN" || upper == "Y") {
        return 'Y';
    }
    if (upper == "Z" || upper == "EIZ") {
        return 'Z';
    }
    if (!upper.empty()) {
        const char suffix = upper.back();
        if (suffix == 'E') {
            return 'X';
        }
        if (suffix == 'N') {
            return 'Y';
        }
        if (suffix == 'Z') {
            return 'Z';
        }
    }
    throw std::runtime_error("cannot infer channel direction from label '"
                             + label + "'");
}

std::map<std::string, std::size_t>
metadata_channel_index(const Metadata &metadata) {
    std::map<std::string, std::size_t> indices;
    for (std::size_t i = 0; i < metadata.InstrumentInfo.Channels.size(); ++i) {
        const auto &id = metadata.InstrumentInfo.Channels[i].ChannelID;
        if (id.empty() || id == "UNKNOWN") {
            std::ostringstream oss;
            oss << "metadata ChannelID at index " << (i + 1)
                << " is empty or UNKNOWN; default external import mapping "
                   "requires IDs such as X1/Y1/Z1";
            throw std::runtime_error(oss.str());
        }
        const auto [_, inserted] = indices.emplace(id, i);
        if (!inserted) {
            throw std::runtime_error("duplicate metadata ChannelID: " + id);
        }
    }
    return indices;
}

std::vector<std::string> metadata_channel_labels(const Metadata &metadata);

std::size_t checked_value_count(std::size_t channel_count,
                                std::size_t sample_count,
                                ValidationReport *report = nullptr);

ExternalDataset merge_filename_order_direction_dataset(
    const std::vector<ExternalDataset> &datasets,
    const std::vector<std::filesystem::path> &files,
    const Metadata &metadata,
    std::string source_format) {
    if (datasets.empty() || datasets.size() != files.size()) {
        throw std::runtime_error("internal error while merging external files");
    }

    const auto channel_indices = metadata_channel_index(metadata);
    const auto channel_count =
        static_cast<std::size_t>(metadata.InstrumentInfo.ChannelNum);
    if (metadata.InstrumentInfo.Channels.size() != channel_count) {
        std::ostringstream oss;
        oss << "InstrumentInfo.ChannelNum ("
            << metadata.InstrumentInfo.ChannelNum
            << ") does not match metadata channel list size ("
            << metadata.InstrumentInfo.Channels.size() << ")";
        throw std::runtime_error(oss.str());
    }

    const auto sample_count = datasets.front().sample_count;
    const auto sample_rate_hz = datasets.front().sample_rate_hz;
    for (std::size_t i = 0; i < datasets.size(); ++i) {
        const auto &dataset = datasets[i];
        if (dataset.sample_count != sample_count) {
            std::ostringstream oss;
            oss << "sample count mismatch while merging "
                << files[i].filename().string() << ": " << dataset.sample_count
                << " vs " << sample_count << " in "
                << files.front().filename().string();
            throw std::runtime_error(oss.str());
        }
        if (!almost_equal(dataset.sample_rate_hz, sample_rate_hz)) {
            std::ostringstream oss;
            oss << "sample rate mismatch while merging "
                << files[i].filename().string() << ": "
                << dataset.sample_rate_hz << " Hz vs " << sample_rate_hz
                << " Hz in " << files.front().filename().string();
            throw std::runtime_error(oss.str());
        }
        const auto expected_values =
            checked_value_count(dataset.channel_count, dataset.sample_count);
        if (dataset.channel_labels.size() != dataset.channel_count
            || dataset.channel_sequential_data.size() != expected_values) {
            throw std::runtime_error("invalid external dataset shape for "
                                     + files[i].string());
        }
    }

    ExternalDataset merged;
    merged.source_format = std::move(source_format);
    merged.channel_count = channel_count;
    merged.sample_count = sample_count;
    merged.sample_rate_hz = sample_rate_hz;
    merged.channel_labels = metadata_channel_labels(metadata);
    merged.channel_sequential_data.assign(
        checked_value_count(channel_count, sample_count), 0.0);
    std::vector<bool> filled(channel_count, false);

    for (std::size_t file_index = 0; file_index < datasets.size();
         ++file_index) {
        const auto &dataset = datasets[file_index];
        for (std::size_t channel_index = 0;
             channel_index < dataset.channel_count;
             ++channel_index) {
            const char direction =
                canonical_direction(dataset.channel_labels[channel_index]);
            const std::string target_id =
                std::string(1, direction) + std::to_string(file_index + 1);
            const auto target = channel_indices.find(target_id);
            if (target == channel_indices.end()) {
                std::ostringstream oss;
                oss << "Cannot map " << files[file_index].filename().string()
                    << " channel '" << dataset.channel_labels[channel_index]
                    << "' to metadata ChannelID '" << target_id << "'";
                throw std::runtime_error(oss.str());
            }
            if (filled[target->second]) {
                throw std::runtime_error("multiple external channels map to "
                                         "metadata ChannelID "
                                         + target_id);
            }

            const auto source_offset = channel_index * sample_count;
            const auto target_offset = target->second * sample_count;
            std::copy_n(dataset.channel_sequential_data.begin()
                            + static_cast<std::ptrdiff_t>(source_offset),
                        sample_count,
                        merged.channel_sequential_data.begin()
                            + static_cast<std::ptrdiff_t>(target_offset));
            filled[target->second] = true;
        }
    }

    for (std::size_t i = 0; i < filled.size(); ++i) {
        if (!filled[i]) {
            std::ostringstream oss;
            oss << "No external channel mapped to metadata ChannelID '"
                << merged.channel_labels[i] << "'";
            throw std::runtime_error(oss.str());
        }
    }
    return merged;
}

std::vector<std::string> metadata_channel_labels(const Metadata &metadata) {
    std::vector<std::string> labels;
    labels.reserve(metadata.InstrumentInfo.Channels.size());
    for (const auto &channel : metadata.InstrumentInfo.Channels) {
        labels.push_back(channel.ChannelID);
    }
    return labels;
}

std::size_t checked_value_count(std::size_t channel_count,
                                std::size_t sample_count,
                                ValidationReport *report) {
    if (channel_count != 0
        && sample_count
               > std::numeric_limits<std::size_t>::max() / channel_count) {
        const std::string message = "channel count and sample count overflow";
        if (report != nullptr) {
            add_error(*report, message);
            return 0;
        }
        throw std::runtime_error(message);
    }
    return channel_count * sample_count;
}

} // namespace

ExternalDataset load_tdms_dataset(const std::string &input_path,
                                  const TdmsImportOptions &options) {
    if (!(options.sensitivity_storage_scale > 0.0)
        || !std::isfinite(options.sensitivity_storage_scale)) {
        throw std::runtime_error(
            "TDMS sensitivity storage scale must be positive and finite");
    }
    if (!std::isfinite(options.post_scale)) {
        throw std::runtime_error("TDMS post scale must be finite");
    }

    tdms::LoadOptions load_options;
    load_options.sensitivity_selection =
        to_tdms_selection(options.sensitivity_selection);
    load_options.explicit_sensitivity = options.explicit_sensitivity;
    load_options.verify_time_axis = options.verify_time_axis;
    load_options.keep_timestamps = true;
    load_options.require_sensitivity = !options.output_counts;

    const auto dataset = tdms::load_dataset(input_path, load_options);
    const auto samples = dataset.north_counts.size();
    ExternalDataset result;
    result.source_format = "tdms";
    result.channel_count = 3;
    result.sample_count = samples;
    result.sample_rate_hz = dataset.sample_rate_hz;
    result.channel_labels = {"N", "E", "Z"};
    result.channel_sequential_data.reserve(samples * result.channel_count);

    const double actual_sensitivity =
        dataset.selected_sensitivity_raw / options.sensitivity_storage_scale;
    const double output_unit_scale = unit_scale(options.output_unit);
    append_channel(result.channel_sequential_data,
                   dataset.north_counts,
                   actual_sensitivity,
                   output_unit_scale,
                   options.post_scale,
                   options.output_counts);
    append_channel(result.channel_sequential_data,
                   dataset.east_counts,
                   actual_sensitivity,
                   output_unit_scale,
                   options.post_scale,
                   options.output_counts);
    append_channel(result.channel_sequential_data,
                   dataset.vertical_counts,
                   actual_sensitivity,
                   output_unit_scale,
                   options.post_scale,
                   options.output_counts);
    return result;
}

ExternalDataset load_tdms_collection(const std::string &input_path,
                                     const Metadata &metadata,
                                     const TdmsImportOptions &options) {
    const auto files = collect_input_files(input_path, {".tdms"}, "TDMS");
    std::vector<ExternalDataset> datasets;
    datasets.reserve(files.size());
    for (const auto &file : files) {
        try {
            datasets.push_back(load_tdms_dataset(file.string(), options));
        } catch (const std::exception &e) {
            throw std::runtime_error("Failed to read TDMS file "
                                     + file.filename().string() + ": "
                                     + e.what());
        }
    }
    return merge_filename_order_direction_dataset(
        datasets,
        files,
        metadata,
        files.size() == 1 ? "tdms" : "tdms-collection");
}

ExternalDataset load_mseed_dataset(const std::string &input_path,
                                   const MseedImportOptions &options) {
    mseed::LoadOptions load_options;
    load_options.include_dimensionless = options.include_dimensionless;
    load_options.verify_time_continuity = options.verify_time_continuity;

    const auto groups = mseed::load_channel_groups(input_path, load_options);
    if (options.group_index >= groups.size()) {
        std::ostringstream oss;
        oss << "MiniSEED group index " << options.group_index
            << " is out of range; file contains " << groups.size()
            << " synchronized group(s)";
        throw std::runtime_error(oss.str());
    }

    const auto &group = groups[options.group_index];
    const auto order = mseed::ordered_channel_indices(group);
    ExternalDataset result;
    result.source_format = "modified-miniseed";
    result.channel_count = order.size();
    result.sample_count = group.sample_count();
    result.sample_rate_hz = group.sample_rate_hz;
    result.channel_labels.reserve(order.size());
    result.channel_sequential_data.reserve(result.channel_count
                                           * result.sample_count);
    for (const auto index : order) {
        const auto &channel = group.channels[index];
        result.channel_labels.push_back(channel.channel);
        result.channel_sequential_data.insert(
            result.channel_sequential_data.end(),
            channel.values.begin(),
            channel.values.end());
    }
    return result;
}

ExternalDataset load_mseed_collection(const std::string &input_path,
                                      const Metadata &metadata,
                                      const MseedImportOptions &options) {
    const auto files =
        collect_input_files(input_path, {".mseed", ".miniseed"}, "MiniSEED");
    std::vector<ExternalDataset> datasets;
    datasets.reserve(files.size());
    for (const auto &file : files) {
        try {
            datasets.push_back(load_mseed_dataset(file.string(), options));
        } catch (const std::exception &e) {
            throw std::runtime_error("Failed to read MiniSEED file "
                                     + file.filename().string() + ": "
                                     + e.what());
        }
    }
    return merge_filename_order_direction_dataset(
        datasets,
        files,
        metadata,
        files.size() == 1 ? "modified-miniseed"
                          : "modified-miniseed-collection");
}

ExternalDataset load_hdf5_dataset(const std::string &input_path,
                                  Metadata *metadata) {
    Hdf5Reader reader;
    reader.open(input_path);
    Metadata file_metadata = reader.read_metadata();

    ExternalDataset result;
    result.source_format = "hdf5";
    result.channel_count = reader.get_channel_num();
    result.sample_count = reader.get_npts();
    result.sample_rate_hz = metadata_sample_rate_hz(file_metadata);
    result.channel_labels = metadata_channel_labels(file_metadata);
    result.channel_sequential_data = reader.read_accform();

    if (metadata != nullptr) {
        *metadata = std::move(file_metadata);
    }
    return result;
}

void write_hdf5_dataset(const std::string &output_path,
                        const Metadata &metadata,
                        const std::vector<double> &channel_sequential_data) {
    const auto report = validate_metadata(metadata);
    if (!report.ok()) {
        std::ostringstream oss;
        oss << "Cannot export invalid qREST metadata to HDF5:";
        for (const auto &error : report.errors) {
            oss << "\n  - " << error;
        }
        throw std::runtime_error(oss.str());
    }

    const auto channel_count =
        static_cast<std::size_t>(metadata.InstrumentInfo.ChannelNum);
    const auto sample_count = static_cast<std::size_t>(metadata.DataInfo.NPTS);
    const auto expected_values =
        checked_value_count(channel_count, sample_count);
    if (channel_sequential_data.size() != expected_values) {
        std::ostringstream oss;
        oss << "Cannot export qREST data to HDF5: expected " << expected_values
            << " values, got " << channel_sequential_data.size();
        throw std::runtime_error(oss.str());
    }

    Hdf5Writer writer;
    writer.open(output_path);
    writer.write(
        metadata, channel_sequential_data, sample_count, channel_count);
}

ValidationReport
validate_external_dataset_compatibility(const ExternalDataset &dataset,
                                        const Metadata &metadata) {
    ValidationReport report = validate_metadata(metadata);
    if (!report.ok()) {
        return report;
    }

    const auto metadata_channels =
        static_cast<std::size_t>(metadata.InstrumentInfo.ChannelNum);
    const auto metadata_samples =
        static_cast<std::size_t>(metadata.DataInfo.NPTS);

    if (dataset.channel_count != metadata_channels) {
        std::ostringstream oss;
        oss << dataset.source_format << " channel count ("
            << dataset.channel_count
            << ") does not match InstrumentInfo.ChannelNum ("
            << metadata.InstrumentInfo.ChannelNum << ")";
        add_error(report, oss.str());
    }
    if (dataset.sample_count != metadata_samples) {
        std::ostringstream oss;
        oss << dataset.source_format << " sample count ("
            << dataset.sample_count << ") does not match DataInfo.NPTS ("
            << metadata.DataInfo.NPTS << ")";
        add_error(report, oss.str());
    }

    const std::size_t expected_values = checked_value_count(
        dataset.channel_count, dataset.sample_count, &report);
    if (expected_values != 0
        && dataset.channel_sequential_data.size() != expected_values) {
        std::ostringstream oss;
        oss << dataset.source_format << " value count mismatch: expected "
            << expected_values << ", got "
            << dataset.channel_sequential_data.size();
        add_error(report, oss.str());
    }

    const double expected_sample_rate = metadata_sample_rate_hz(metadata);
    if (dataset.sample_rate_hz > 0.0
        && !almost_equal(dataset.sample_rate_hz, expected_sample_rate)) {
        std::ostringstream oss;
        oss << dataset.source_format << " sample rate ("
            << dataset.sample_rate_hz
            << " Hz) does not match DataInfo.DT-derived sample rate ("
            << expected_sample_rate << " Hz)";
        add_error(report, oss.str());
    }

    if (dataset.channel_labels.size()
        == metadata.InstrumentInfo.Channels.size()) {
        for (std::size_t i = 0; i < dataset.channel_labels.size(); ++i) {
            const auto &expected =
                metadata.InstrumentInfo.Channels[i].ChannelID;
            if (!expected.empty() && expected != "UNKNOWN"
                && expected != dataset.channel_labels[i]) {
                std::ostringstream oss;
                oss << "External channel " << (i + 1) << " label is '"
                    << dataset.channel_labels[i]
                    << "' but metadata ChannelID is '" << expected << "'";
                add_warning(report, oss.str());
            }
        }
    }

    return report;
}

void require_external_dataset_compatibility(const ExternalDataset &dataset,
                                            const Metadata &metadata) {
    const auto report =
        validate_external_dataset_compatibility(dataset, metadata);
    if (report.ok()) {
        return;
    }

    std::ostringstream oss;
    oss << "External dataset is not compatible with metadata:";
    for (const auto &error : report.errors) {
        oss << "\n  - " << error;
    }
    throw std::runtime_error(oss.str());
}

} // namespace qrest_data::tools
