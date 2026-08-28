#include "external_import.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
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
                                ValidationReport *report = nullptr) {
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
