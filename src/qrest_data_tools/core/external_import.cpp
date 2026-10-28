/**
**            qREST - Quick Response Evaluation for Safety Tagging
**     Institute of Engineering Mechanics, China Earthquake Administration
**                 Copyright 2024 - 2026 QLab, Dong Feiyue
**                          All Rights Reserved.
** Project: qREST
** File: /src/qrest_data_tools/core/external_import.cpp
** File Created: not recorded in existing source
** Author: not recorded in existing source
** Last Modified: Wednesday, 28 October 2026
** Modified By: Codex
*/
#include "external_import.hpp"
#include "channel_import.hpp"
#include "text_matrix.hpp"

#include <algorithm>
#include <cctype>
#include <chrono>
#include <cmath>
#include <filesystem>
#include <iomanip>
#include <limits>
#include <set>
#include <sstream>
#include <stdexcept>
#include <utility>

#include <nlohmann/json.hpp>
#include "../../qrest_data_hdf5/hdf5_reader.hpp"
#include "../../qrest_data_hdf5/hdf5_writer.hpp"
#include "../formats/mseed/modified_mseed_export.hpp"
#include "../formats/sit/sit_dat_reader.hpp"
#include "../formats/sit/sit_txt_reader.hpp"
#include "../formats/tdms/tdms_export.hpp"
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

mseed::GapPolicy to_mseed_gap_policy(MseedImportOptions::GapPolicy policy) {
    switch (policy) {
        case MseedImportOptions::GapPolicy::FillNaN:
            return mseed::GapPolicy::FillNaN;
        case MseedImportOptions::GapPolicy::Error:
            return mseed::GapPolicy::Error;
        case MseedImportOptions::GapPolicy::Ignore:
            return mseed::GapPolicy::Ignore;
    }
    throw std::runtime_error("unsupported MiniSEED gap policy");
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

std::uint64_t tdms_start_time_ms(const tdms::Timestamp &timestamp) {
    const long double ms = tdms::timestamp_unix_seconds(timestamp) * 1000.0L;
    if (ms < 0.0L) {
        throw std::runtime_error("TDMS start timestamp is before Unix epoch");
    }
    return static_cast<std::uint64_t>(std::llround(ms));
}

std::optional<std::uint64_t>
consistent_start_time(const std::vector<ExternalDataset> &datasets) {
    if (datasets.empty() || !datasets.front().start_time_ms.has_value()) {
        return std::nullopt;
    }
    const std::uint64_t first = *datasets.front().start_time_ms;
    for (const auto &dataset : datasets) {
        if (!dataset.start_time_ms.has_value()
            || *dataset.start_time_ms != first) {
            return std::nullopt;
        }
    }
    return first;
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

std::vector<std::string> metadata_channel_labels(const Metadata &metadata);

std::size_t checked_value_count(std::size_t channel_count,
                                std::size_t sample_count,
                                ValidationReport *report = nullptr);

ExternalDataset
merge_filename_order_dataset(const std::vector<ExternalDataset> &datasets,
                             const std::vector<std::filesystem::path> &files,
                             std::string source_format) {
    if (datasets.empty() || datasets.size() != files.size()) {
        throw std::runtime_error("internal error while merging external files");
    }

    if (datasets.size() == 1)
        return datasets.front();
    const bool strict =
        std::any_of(datasets.begin(), datasets.end(), [](const auto &d) {
            return d.source_format != "tdms";
        });
    std::set<std::string> ids;
    if (strict)
        for (const auto &d : datasets) {
            if (d.start_time_us != datasets.front().start_time_us)
                throw std::runtime_error(
                    "directory inputs have different UTC start times; time "
                    "segments must be assembled by record time");
            for (const auto &c : d.channels)
                if (!ids.insert(c.source_id).second)
                    throw std::runtime_error(
                        "duplicate source channel in directory: "
                        + c.source_id);
        }
    const auto sample_count = datasets.front().sample_count;
    const auto sample_rate_hz = datasets.front().sample_rate_hz;
    std::size_t channel_count = 0;
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
        channel_count += dataset.channel_count;
    }

    ExternalDataset merged;
    merged.source_format = std::move(source_format);
    merged.channel_count = channel_count;
    merged.sample_count = sample_count;
    merged.sample_rate_hz = sample_rate_hz;
    merged.start_time_ms = consistent_start_time(datasets);
    merged.start_time_us = datasets.front().start_time_us;
    merged.representation = datasets.front().representation;
    merged.output_unit = datasets.front().output_unit;
    merged.time_basis = datasets.front().time_basis;
    for (const auto &d : datasets) {
        merged.source_group_count =
            std::max(merged.source_group_count, d.source_group_count);
        if (d.representation != merged.representation)
            throw std::runtime_error(
                "conflicting representations in directory");
        merged.channels.insert(
            merged.channels.end(), d.channels.begin(), d.channels.end());
        merged.diagnostics.insert(merged.diagnostics.end(),
                                  d.diagnostics.begin(),
                                  d.diagnostics.end());
    }
    merged.distance_unit = datasets.front().distance_unit;
    for (const auto &d : datasets)
        if (d.distance_unit != merged.distance_unit)
            throw std::runtime_error(
                "External files declare conflicting units");
    merged.channel_labels.reserve(channel_count);
    merged.channel_sequential_data.assign(
        checked_value_count(channel_count, sample_count), 0.0);

    std::size_t target_channel = 0;
    for (std::size_t file_index = 0; file_index < datasets.size();
         ++file_index) {
        const auto &dataset = datasets[file_index];
        for (std::size_t channel_index = 0;
             channel_index < dataset.channel_count;
             ++channel_index) {
            merged.channel_labels.push_back(
                files.size() == 1
                    ? dataset.channel_labels[channel_index]
                    : files[file_index].filename().string() + "/"
                          + dataset.channel_labels[channel_index]);
            if (dataset.channel_measurands.size() == dataset.channel_count)
                merged.channel_measurands.push_back(
                    dataset.channel_measurands[channel_index]);
            const auto source_offset = channel_index * sample_count;
            const auto target_offset = target_channel * sample_count;
            std::copy_n(dataset.channel_sequential_data.begin()
                            + static_cast<std::ptrdiff_t>(source_offset),
                        sample_count,
                        merged.channel_sequential_data.begin()
                            + static_cast<std::ptrdiff_t>(target_offset));
            ++target_channel;
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

void validate_source_contract(
    ValidationReport &report,
    const ExternalDataset &d,
    const Metadata &metadata,
    const std::vector<ExternalChannelMapping> &mapping) {
    if (d.representation.empty())
        return; // existing TDMS/HDF5 contracts are unchanged
    if (d.start_time_us) {
        try {
            const auto t =
                parse_iso8601_timestamp_ms(metadata.DataInfo.StartTime);
            if (static_cast<long double>(t) * 1000 != *d.start_time_us)
                add_error(report,
                          "source UTC start time conflicts with metadata "
                          "StartTime (or loses sub-millisecond precision)");
        } catch (const std::exception &e) {
            add_error(report, e.what());
        }
    }
    if (d.representation == "RawCounts") {
        if (metadata.Units[0] != "count")
            add_error(report, "RawCounts require metadata Units[0]=count");
    } else if (!d.distance_unit.empty()
               && d.distance_unit != metadata.Units[0]) {
        add_error(report,
                  "source/output unit conflicts with metadata Units[0]");
    }
    for (const auto &m : mapping) {
        if (m.source_channel >= d.channel_count
            || m.target_channel >= metadata.InstrumentInfo.Channels.size())
            continue;
        const auto &target = metadata.InstrumentInfo.Channels[m.target_channel];
        if (d.channel_measurands.size() == d.channel_count
            && d.channel_measurands[m.source_channel] != target.Measurand)
            add_error(report,
                      "source quantity conflicts with target Measurand for "
                          + target.ChannelID);
        if (target.Scale != 1.0)
            add_error(report,
                      "converted external data requires metadata Scale=1");
        const auto source_axis = d.channels.size() == d.channel_count
                                     ? d.channels[m.source_channel].axis_hint
                                     : "Unknown";
        const auto target_axis = external_axis_hint(target.ChannelID);
        if (source_axis != "Unknown" && target_axis != "Unknown"
            && source_axis != target_axis)
            add_error(report,
                      "source/target direction conflict for " + target.ChannelID
                          + "; supply an explicit correct mapping");
    }
}

void add_non_finite_data_errors(ValidationReport &report,
                                const ExternalDataset &dataset) {
    std::size_t count = 0;
    for (double value : dataset.channel_sequential_data) {
        if (!std::isfinite(value)) {
            ++count;
        }
    }
    if (count == 0) {
        return;
    }

    std::ostringstream oss;
    oss << dataset.source_format << " contains " << count
        << " non-finite sample value(s); qREST import requires finite data";
    add_error(report, oss.str());
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
    if (!dataset.timestamps.empty()) {
        result.start_time_ms = tdms_start_time_ms(dataset.timestamps.front());
    }
    result.channel_labels = {"N", "E", "Z"};
    result.channel_measurands = {
        "Acceleration", "Acceleration", "Acceleration"};
    if (!options.output_counts)
        result.distance_unit =
            options.output_unit
                    == TdmsImportOptions::Unit::MeterPerSecondSquared
                ? "m"
                : "cm";
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

ExternalDataset load_tdms_external_data(const std::string &input_path,
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
    return merge_filename_order_dataset(
        datasets, files, files.size() == 1 ? "tdms" : "tdms-collection");
}

ExternalDataset load_tdms_collection(const std::string &input_path,
                                     const Metadata &metadata,
                                     const TdmsImportOptions &options) {
    const auto merged = load_tdms_external_data(input_path, options);
    return apply_external_channel_mapping(
        merged, metadata, make_sequential_channel_mapping(merged, metadata));
}

namespace {
mseed::Dimension parse_quantity(const std::string &q) {
    if (q.empty())
        return mseed::Dimension::Unknown;
    if (q == "Acceleration")
        return mseed::Dimension::Acceleration;
    if (q == "Velocity")
        return mseed::Dimension::Velocity;
    if (q == "Displacement")
        return mseed::Dimension::Displacement;
    throw std::runtime_error("unsupported physical quantity: " + q);
}
mseed::LoadOptions mseed_options(const MseedImportOptions &options) {
    mseed::LoadOptions o;
    o.include_dimensionless = options.include_dimensionless;
    o.gap_policy = to_mseed_gap_policy(options.gap_policy);
    o.output_counts = options.output_counts;
    o.sensitivity = options.sensitivity;
    o.quantity = parse_quantity(options.quantity);
    if (options.output_counts
        && (options.require_physical || !options.quantity.empty()))
        throw std::runtime_error(
            "counts and physical output options are mutually exclusive");
    return o;
}
ExternalDataset group_dataset(const std::vector<mseed::ChannelGroup> &groups,
                              const MseedImportOptions &options,
                              bool dat) {
    if (options.group_index >= groups.size())
        throw std::runtime_error("MiniSEED group index out of range; groups="
                                 + std::to_string(groups.size()));
    const auto &group = groups[options.group_index];
    ExternalDataset result;
    result.source_format =
        dat ? "sit-dat" : group.channels.front().source_format;
    result.source_group_count = groups.size();
    result.channel_count = group.channels.size();
    result.sample_count = group.sample_count();
    result.sample_rate_hz = group.sample_rate_hz;
    result.start_time_us = group.start_time_us;
    if (group.start_time_us >= 0)
        result.start_time_ms = std::uint64_t(group.start_time_us / 1000);
    result.time_basis =
        "UTC (MiniSEED BTime, time correction and Blockette 1001)";
    const bool raw = group.channels.front().raw_counts;
    result.representation = raw ? "RawCounts" : "Physical";
    if (raw && options.require_physical)
        throw std::runtime_error("raw counts decoded; physical output requires "
                                 "reliable sensitivity and quantity");
    const bool cm = options.output_unit == MotionUnit::Centimeter;
    result.output_unit =
        raw ? "count" : mseed::physical_unit_name(group.dimension);
    if (!raw && cm)
        result.output_unit = "c" + result.output_unit;
    if (!raw && group.dimension != mseed::Dimension::Dimensionless)
        result.distance_unit = cm ? "cm" : "m";
    if (raw)
        result.diagnostics.push_back(
            "RawCounts: no physical conversion applied; acceleration metadata "
            "is forbidden");
    if (options.gap_policy == MseedImportOptions::GapPolicy::Ignore)
        result.diagnostics.push_back("gap-policy=ignore: record concatenation "
                                     "can destroy the true time axis");
    for (const auto &channel : group.channels) {
        if (channel.raw_counts != raw)
            throw std::runtime_error("mixed RawCounts/Physical channels "
                                     "require separate import groups");
        result.channel_labels.push_back(channel.channel);
        auto quantity = mseed::dimension_name(channel.dimension);
        if (!quantity.empty())
            quantity[0] = static_cast<char>(
                std::toupper(static_cast<unsigned char>(quantity[0])));
        result.channel_measurands.push_back(raw ? "RawCounts" : quantity);
        result.channels.push_back(
            {channel.network + "." + channel.station + "." + channel.location
                 + "." + channel.channel,
             channel.channel,
             external_axis_hint(channel.channel),
             channel.source_order,
             channel.start_time_us,
             channel.record_count,
             channel.missing_sample_count,
             channel.actual_sensitivity > 0
                 ? std::optional<double>{channel.actual_sensitivity}
                 : std::nullopt,
             channel.sensitivity_origin,
             "count",
             quantity});
        for (double value : channel.values)
            result.channel_sequential_data.push_back(
                value * (!raw && cm ? 100.0 : 1.0));
    }
    return result;
}
} // namespace
ExternalDataset load_mseed_dataset(const std::string &path,
                                   const MseedImportOptions &options) {
    return group_dataset(
        mseed::load_channel_groups(path, mseed_options(options)),
        options,
        false);
}
ExternalDataset load_sit_dat_dataset(const std::string &path,
                                     const SitDatImportOptions &options) {
    return group_dataset(
        mseed::assemble_channel_groups(sit::read_dat_records(path),
                                       mseed_options(options)),
        options,
        true);
}
ExternalDataset load_sit_txt_dataset(const std::string &path,
                                     const SitTxtImportOptions &options) {
    const auto text = sit::read_txt(path, options.timezone);
    ExternalDataset d;
    d.source_format = "sit-txt";
    d.channel_count = 3;
    d.sample_count = text.values[0].size();
    d.sample_rate_hz = text.sample_rate_hz;
    d.start_time_us = text.start_time_us;
    if (text.start_time_us >= 0)
        d.start_time_ms = std::uint64_t(text.start_time_us / 1000);
    d.time_basis = "SIT Gal profile, source timezone " + options.timezone
                   + ", header " + text.encoding;
    d.representation = "Physical";
    const bool cm = options.output_unit == MotionUnit::Centimeter;
    d.distance_unit = cm ? "cm" : "m";
    d.output_unit = cm ? "cm/s^2" : "m/s^2";
    for (std::size_t i = 0; i < 3; ++i) {
        d.channel_labels.push_back(text.labels[i]);
        d.channel_measurands.push_back("Acceleration");
        d.channels.push_back({text.labels[i],
                              text.labels[i],
                              external_axis_hint(text.labels[i]),
                              i,
                              text.start_time_us,
                              0,
                              0,
                              std::nullopt,
                              "not-applicable (physical source)",
                              "Gal (SIT profile)"});
        for (double v : text.values[i])
            d.channel_sequential_data.push_back(v * (cm ? 1.0 : 0.01));
    }
    return d;
}
std::string external_axis_hint(const std::string &s) {
    if (s == "EIN" || s == "NS" || s == "N")
        return "North";
    if (s == "EIE" || s == "EW" || s == "E")
        return "East";
    if (s == "EIZ" || s == "UD" || s == "Z")
        return "Vertical";
    return "Unknown";
}
namespace {
std::string utc_string(std::int64_t t) {
    using namespace std::chrono;
    const sys_time<microseconds> tp{microseconds{t}};
    const auto day = floor<days>(tp);
    const year_month_day d{day};
    const hh_mm_ss clock{tp - day};
    std::ostringstream out;
    out << std::setfill('0') << std::setw(4) << int(d.year()) << '-'
        << std::setw(2) << unsigned(d.month()) << '-' << std::setw(2)
        << unsigned(d.day()) << 'T' << std::setw(2) << clock.hours().count()
        << ':' << std::setw(2) << clock.minutes().count() << ':' << std::setw(2)
        << clock.seconds().count() << '.' << std::setw(6)
        << clock.subseconds().count() << 'Z';
    return out.str();
}
} // namespace
std::string external_dataset_summary_json(const ExternalDataset &d) {
    nlohmann::json j = {{"source_format", d.source_format},
                        {"channels", d.channel_count},
                        {"samples", d.sample_count},
                        {"sample_rate_hz", d.sample_rate_hz},
                        {"representation", d.representation},
                        {"output_unit", d.output_unit},
                        {"time_basis", d.time_basis},
                        {"diagnostics", d.diagnostics},
                        {"channel_list", nlohmann::json::array()}};
    if (d.start_time_us) {
        j["start_time_us"] = *d.start_time_us;
        j["start_time_utc"] = utc_string(*d.start_time_us);
        if (d.sample_count && d.sample_rate_hz > 0)
            j["last_sample_time_us"] =
                *d.start_time_us
                + std::llround((d.sample_count - 1) * 1000000.0
                               / d.sample_rate_hz);
    }
    j["record_count"] = 0;
    j["missing_samples"] = 0;
    j["overlap_records"] = 0;
    j["undecodable_records"] = 0; // fatal errors never produce a dataset
    for (const auto &c : d.channels) {
        j["record_count"] =
            j["record_count"].get<std::uint64_t>() + c.record_count;
        j["missing_samples"] =
            j["missing_samples"].get<std::uint64_t>() + c.missing_samples;
    }
    j["non_finite_samples"] =
        std::count_if(d.channel_sequential_data.begin(),
                      d.channel_sequential_data.end(),
                      [](double x) { return !std::isfinite(x); });
    for (const auto &c : d.channels) {
        nlohmann::json channel = {
            {"source_id", c.source_id},
            {"source_label", c.source_label},
            {"source_order", c.source_order},
            {"axis_hint", c.axis_hint},
            {"start_time_us", c.start_time_us},
            {"records", c.record_count},
            {"missing_samples", c.missing_samples},
            {"sensitivity_origin", c.sensitivity_origin},
            {"sensitivity_unit",
             c.sensitivity ? "count/SI-unit" : "unknown/not-applicable"},
            {"source_unit", c.source_unit}};
        channel["sensitivity"] = c.sensitivity ? nlohmann::json(*c.sensitivity)
                                               : nlohmann::json(nullptr);
        j["channel_list"].push_back(std::move(channel));
    }
    return j.dump(2);
}

ExternalDataset load_mseed_external_data(const std::string &input_path,
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
    if (datasets.size() == 1)
        return std::move(datasets.front());
    return merge_filename_order_dataset(datasets, files, "miniseed-collection");
}

ExternalDataset load_mseed_collection(const std::string &input_path,
                                      const Metadata &metadata,
                                      const MseedImportOptions &options) {
    const auto merged = load_mseed_external_data(input_path, options);
    return apply_external_channel_mapping(
        merged, metadata, make_sequential_channel_mapping(merged, metadata));
}

ExternalDataset read_external_dataset(const std::string &path,
                                      const std::string &format,
                                      const std::string &options_json) {
    const auto j = nlohmann::json::parse(options_json);
    if (!j.is_object())
        throw std::runtime_error("reader options must be a JSON object");
    const std::set<std::string> known = {"unit",
                                         "counts",
                                         "sensitivity",
                                         "quantity",
                                         "timezone",
                                         "gap_policy",
                                         "group_index",
                                         "include_dimensionless"};
    for (const auto &[key, value] : j.items()) {
        (void)value;
        if (!known.contains(key))
            throw std::runtime_error("unknown reader option: " + key);
    }
    if (format != "mseed" && format != "sit-dat" && format != "sit-txt")
        throw std::runtime_error("unsupported reader format: " + format);
    const auto unit = j.value("unit", format == "mseed" ? "m/s2" : "cm/s2");
    if (unit != "m/s2" && unit != "cm/s2")
        throw std::runtime_error("unit must be m/s2 or cm/s2");
    const auto output =
        unit == "m/s2" ? MotionUnit::SI : MotionUnit::Centimeter;
    if (format == "sit-txt") {
        for (const auto &[key, value] : j.items()) {
            (void)value;
            if (key != "unit" && key != "timezone")
                throw std::runtime_error("inapplicable SIT TXT option: " + key);
        }
        const auto files = collect_input_files(path, {".txt"}, "SIT TXT");
        std::vector<ExternalDataset> datasets;
        for (const auto &f : files)
            datasets.push_back(load_sit_txt_dataset(
                f.string(), {output, j.value("timezone", "+08:00")}));
        return merge_filename_order_dataset(
            datasets, files, "sit-txt-collection");
    }
    if (j.contains("timezone"))
        throw std::runtime_error("timezone applies only to SIT TXT");
    MseedImportOptions o;
    o.output_unit = output;
    o.output_counts = j.value("counts", false);
    o.require_physical = j.contains("unit") || j.contains("sensitivity")
                         || j.contains("quantity");
    if (j.contains("group_index")
        && (!j["group_index"].is_number_integer()
            || j["group_index"].get<long long>() < 0))
        throw std::runtime_error("group_index must be a nonnegative integer");
    o.group_index = j.value("group_index", std::size_t{0});
    o.include_dimensionless = j.value("include_dimensionless", false);
    if (j.contains("sensitivity"))
        o.sensitivity = j.at("sensitivity").get<double>();
    o.quantity = j.value("quantity", "");
    const auto gap = j.value("gap_policy", "fill-nan");
    if (gap == "error")
        o.gap_policy = MseedImportOptions::GapPolicy::Error;
    else if (gap == "ignore")
        o.gap_policy = MseedImportOptions::GapPolicy::Ignore;
    else if (gap != "fill-nan")
        throw std::runtime_error("unknown gap policy: " + gap);
    const auto finish = [&](ExternalDataset dataset) {
        if (j.contains("unit"))
            for (const auto &q : dataset.channel_measurands)
                if (q != "Acceleration")
                    throw std::runtime_error("acceleration output unit "
                                             "conflicts with source quantity");
        return dataset;
    };
    if (format == "mseed")
        return finish(load_mseed_external_data(path, o));
    const auto files = collect_input_files(path, {".dat"}, "SIT DAT");
    std::vector<ExternalDataset> datasets;
    SitDatImportOptions dat;
    static_cast<MseedImportOptions &>(dat) = o;
    for (const auto &f : files)
        datasets.push_back(load_sit_dat_dataset(f.string(), dat));
    return finish(
        merge_filename_order_dataset(datasets, files, "sit-dat-collection"));
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
    try {
        result.start_time_ms =
            parse_iso8601_timestamp_ms(file_metadata.DataInfo.StartTime);
    } catch (const std::exception &) {
        result.start_time_ms.reset();
    }
    result.channel_labels = metadata_channel_labels(file_metadata);
    result.distance_unit = file_metadata.Units[0];
    for (const auto &c : file_metadata.InstrumentInfo.Channels)
        result.channel_measurands.push_back(c.Measurand);
    result.channel_sequential_data = reader.read_accform();

    if (metadata != nullptr) {
        *metadata = std::move(file_metadata);
    }
    return result;
}

ChannelImport read_channel_import(const std::string &path,
                                  const std::string &format,
                                  bool transpose_text) {
    ChannelImport result;
    result.source = path;
    result.format = format;
    result.transpose_text = format == "text" && transpose_text;
    auto &d = result.data;
    if (format == "text") {
        const auto files = collect_input_files(path, {".txt", ".csv"}, "TXT");
        std::vector<ExternalDataset> inputs;
        for (const auto &file : files) {
            const auto u8 = file.u8string();
            auto matrix = read_channel_text_matrix(
                std::string(u8.begin(), u8.end()), transpose_text);
            ExternalDataset item;
            item.source_format = "text";
            item.channel_count = matrix.channels;
            item.sample_count = matrix.samples;
            item.channel_sequential_data = std::move(matrix.values);
            for (std::size_t c = 0; c < item.channel_count; ++c)
                item.channel_labels.push_back("D" + std::to_string(c + 1));
            inputs.push_back(std::move(item));
        }
        d = merge_filename_order_dataset(inputs, files, "text");
        result.scales.assign(d.channel_count, 1.0);
        result.scale_origins.assign(d.channel_count,
                                    "default (user-declared acceleration)");
    } else if (format == "mseed" || format == "sit-dat") {
        d = read_external_dataset(
            path, format, R"({"counts":true,"gap_policy":"error"})");
        if (d.source_group_count != 1)
            throw std::runtime_error(
                "Source has multiple incompatible channel groups; import a "
                "single synchronous acceleration group");
        for (const auto &c : d.channels) {
            if (c.source_measurand == "Velocity"
                || c.source_measurand == "Displacement"
                || c.source_measurand == "Dimensionless")
                throw std::runtime_error(
                    c.source_id + ": source quantity is not acceleration");
            result.scales.push_back(c.sensitivity ? 1.0 / *c.sensitivity : 1.0);
            result.scale_origins.push_back(
                c.sensitivity ? c.sensitivity_origin
                              : "default (user-declared acceleration)");
        }
    } else if (format == "sit-txt") {
        // The existing SIT profile returns Gal by default, with UTC +08:00.
        d = read_external_dataset(path, format);
        result.scales.assign(d.channel_count, 0.01);
        result.scale_origins.assign(d.channel_count, "SIT Gal profile default");
    } else if (format == "tdms") {
        // Preserve the current TDMS decoder/calibration defaults pending its
        // separate parser work. Its returned values are cm/s^2, not counts.
        d = load_tdms_external_data(path);
        result.scales.assign(d.channel_count, 0.01);
        result.scale_origins.assign(d.channel_count,
                                    "TDMS reader default calibration (cm/s^2)");
    } else if (format == "hdf5") {
        Metadata metadata;
        d = load_hdf5_dataset(path, &metadata);
        const double unit = metadata.Units[0] == "m"    ? 1.0
                            : metadata.Units[0] == "cm" ? 0.01
                                                        : 0;
        if (!unit || metadata.InstrumentInfo.Channels.size() != d.channel_count)
            throw std::runtime_error(
                "HDF5 requires acceleration channels and m/cm units");
        for (const auto &c : metadata.InstrumentInfo.Channels) {
            if (c.Measurand != "Acceleration")
                throw std::runtime_error(
                    "HDF5 source quantity is not acceleration");
            result.scales.push_back(c.Scale * unit);
            result.scale_origins.push_back("HDF5 Metadata Scale and unit");
        }
    } else
        throw std::runtime_error("Unknown channel import format: " + format);
    if (const auto error = channel_import_error(result); !error.empty())
        throw std::runtime_error(path + ": " + error);
    return result;
}

std::vector<ExternalChannelMapping>
make_sequential_channel_mapping(const ExternalDataset &dataset,
                                const Metadata &metadata) {
    const auto target_count =
        static_cast<std::size_t>(metadata.InstrumentInfo.ChannelNum);
    const auto count = std::min(dataset.channel_count, target_count);
    std::vector<ExternalChannelMapping> mapping;
    mapping.reserve(count);
    for (std::size_t i = 0; i < count; ++i) {
        mapping.push_back({i, i});
    }
    return mapping;
}

ValidationReport validate_external_channel_mapping(
    const ExternalDataset &dataset,
    const Metadata &metadata,
    const std::vector<ExternalChannelMapping> &mapping) {
    ValidationReport report = validate_metadata(metadata);
    const auto metadata_channels =
        static_cast<std::size_t>(metadata.InstrumentInfo.ChannelNum);
    const auto metadata_samples =
        static_cast<std::size_t>(metadata.DataInfo.NPTS);

    if (metadata.InstrumentInfo.Channels.size() != metadata_channels) {
        std::ostringstream oss;
        oss << "InstrumentInfo.ChannelNum ("
            << metadata.InstrumentInfo.ChannelNum
            << ") does not match metadata channel list size ("
            << metadata.InstrumentInfo.Channels.size() << ")";
        add_error(report, oss.str());
    }

    if (dataset.channel_count != mapping.size()) {
        std::ostringstream oss;
        oss << "external channel mapping size (" << mapping.size()
            << ") must match external channel count (" << dataset.channel_count
            << ")";
        add_error(report, oss.str());
    }
    if (!dataset.channels.empty()
        && dataset.channels.size() != dataset.channel_count)
        add_error(report,
                  "external channel provenance must match channel count");
    if (dataset.channel_labels.size() != dataset.channel_count) {
        add_error(report,
                  "external channel labels must match external channel count");
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
    if (expected_values != 0
        && dataset.channel_sequential_data.size() == expected_values) {
        add_non_finite_data_errors(report, dataset);
    }

    std::vector<bool> used_sources(dataset.channel_count, false);
    std::vector<bool> used_targets(metadata_channels, false);
    for (const auto &entry : mapping) {
        if (entry.source_channel >= dataset.channel_count) {
            std::ostringstream oss;
            oss << "external channel mapping source index "
                << entry.source_channel << " is out of range";
            add_error(report, oss.str());
            continue;
        }
        if (entry.target_channel >= metadata_channels) {
            std::ostringstream oss;
            oss << "external channel mapping target index "
                << entry.target_channel << " is out of range";
            add_error(report, oss.str());
            continue;
        }
        if (used_sources[entry.source_channel]) {
            std::ostringstream oss;
            oss << "external channel " << (entry.source_channel + 1)
                << " is mapped more than once";
            add_error(report, oss.str());
        }
        if (used_targets[entry.target_channel]) {
            std::ostringstream oss;
            oss << "qREST channel " << (entry.target_channel + 1)
                << " is mapped more than once";
            add_error(report, oss.str());
        }
        used_sources[entry.source_channel] = true;
        used_targets[entry.target_channel] = true;
    }

    for (std::size_t i = 0; i < used_sources.size(); ++i) {
        if (!used_sources[i]) {
            std::ostringstream oss;
            oss << "external channel " << (i + 1) << " is not mapped";
            add_error(report, oss.str());
        }
    }
    for (std::size_t i = 0; i < used_targets.size(); ++i) {
        if (!used_targets[i]) {
            std::ostringstream oss;
            oss << "qREST channel " << (i + 1) << " is not mapped";
            add_error(report, oss.str());
        }
    }

    const double expected_sample_rate = metadata_sample_rate_hz(metadata);
    if (dataset.sample_rate_hz > 0.0 && expected_sample_rate > 0.0
        && !almost_equal(dataset.sample_rate_hz, expected_sample_rate)) {
        std::ostringstream oss;
        oss << dataset.source_format << " sample rate ("
            << dataset.sample_rate_hz
            << " Hz) does not match DataInfo.DT-derived sample rate ("
            << expected_sample_rate << " Hz)";
        add_error(report, oss.str());
    }

    validate_source_contract(report, dataset, metadata, mapping);
    return report;
}

ExternalDataset apply_external_channel_mapping(
    const ExternalDataset &dataset,
    const Metadata &metadata,
    const std::vector<ExternalChannelMapping> &mapping) {
    const auto report =
        validate_external_channel_mapping(dataset, metadata, mapping);
    if (!report.ok()) {
        std::ostringstream oss;
        oss << "External channel mapping is invalid:";
        for (const auto &error : report.errors) {
            oss << "\n  - " << error;
        }
        throw std::runtime_error(oss.str());
    }

    ExternalDataset mapped = dataset;
    mapped.source_format = dataset.source_format;
    mapped.channel_count =
        static_cast<std::size_t>(metadata.InstrumentInfo.ChannelNum);
    mapped.sample_count = dataset.sample_count;
    mapped.sample_rate_hz = dataset.sample_rate_hz;
    mapped.start_time_ms = dataset.start_time_ms;
    mapped.channel_labels = metadata_channel_labels(metadata);
    mapped.distance_unit = dataset.distance_unit;
    if (dataset.channel_measurands.size() == dataset.channel_count)
        mapped.channel_measurands.resize(mapped.channel_count);
    mapped.channel_sequential_data.assign(
        checked_value_count(mapped.channel_count, mapped.sample_count), 0.0);

    if (dataset.channels.size() == dataset.channel_count)
        mapped.channels.resize(mapped.channel_count);
    for (const auto &entry : mapping) {
        if (!dataset.channels.empty())
            mapped.channels[entry.target_channel] =
                dataset.channels[entry.source_channel];
        if (!mapped.channel_measurands.empty())
            mapped.channel_measurands[entry.target_channel] =
                dataset.channel_measurands[entry.source_channel];
        const auto source_offset = entry.source_channel * dataset.sample_count;
        const auto target_offset = entry.target_channel * mapped.sample_count;
        std::copy_n(dataset.channel_sequential_data.begin()
                        + static_cast<std::ptrdiff_t>(source_offset),
                    dataset.sample_count,
                    mapped.channel_sequential_data.begin()
                        + static_cast<std::ptrdiff_t>(target_offset));
    }
    return mapped;
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
    if (expected_values != 0
        && dataset.channel_sequential_data.size() == expected_values) {
        add_non_finite_data_errors(report, dataset);
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

    validate_source_contract(
        report,
        dataset,
        metadata,
        make_sequential_channel_mapping(dataset, metadata));
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
