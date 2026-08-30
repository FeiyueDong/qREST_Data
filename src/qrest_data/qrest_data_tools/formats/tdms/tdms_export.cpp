#include "tdms_export.hpp"

#include <algorithm>
#include <cmath>
#include <fstream>
#include <iomanip>
#include <limits>
#include <optional>
#include <sstream>
#include <stdexcept>

namespace qrest_data::tools::tdms {
namespace {

constexpr const char *kRootPath = "/";
constexpr const char *kSampleRatePath = "/'Parmt'/'SRate'";
constexpr const char *kSensitivityPath = "/'Parmt'/'Sen'";
constexpr const char *kNorthPath = "/'Data'/'N'";
constexpr const char *kEastPath = "/'Data'/'E'";
constexpr const char *kVerticalPath = "/'Data'/'Z'";
constexpr const char *kTimePath = "/'Data'/'Time'";

bool almost_equal(double a, double b, double rel = 1e-10) {
    const double scale = std::max({1.0, std::abs(a), std::abs(b)});
    return std::abs(a - b) <= rel * scale;
}

void append(std::vector<std::int32_t> &dst, std::vector<std::int32_t> &&src) {
    dst.insert(dst.end(), src.begin(), src.end());
}

void append(std::vector<Timestamp> &dst, std::vector<Timestamp> &&src) {
    dst.insert(dst.end(), src.begin(), src.end());
}

std::string metadata_path_for(const std::string &output_path) {
    return output_path + ".meta.txt";
}

void update_waveform_range(Dataset &dataset, std::uint64_t offset) {
    if (dataset.first_waveform_offset == 0
        && dataset.last_waveform_offset == 0) {
        dataset.first_waveform_offset = offset;
        dataset.last_waveform_offset = offset;
        return;
    }
    dataset.first_waveform_offset =
        std::min(dataset.first_waveform_offset, offset);
    dataset.last_waveform_offset =
        std::max(dataset.last_waveform_offset, offset);
}

std::optional<double>
value_active_before(const std::vector<ParameterEvent> &events,
                    std::uint64_t waveform_offset) {
    std::optional<double> value;
    for (const auto &event : events) {
        if (event.file_offset <= waveform_offset) {
            value = event.value;
        } else {
            break;
        }
    }
    return value;
}

void verify_no_parameter_change_during_waveform(
    const std::vector<ParameterEvent> &events,
    std::uint64_t first_offset,
    std::uint64_t last_offset,
    double expected,
    const char *parameter_name) {
    for (const auto &event : events) {
        if (event.file_offset > first_offset && event.file_offset <= last_offset
            && !almost_equal(event.value, expected)) {
            std::ostringstream oss;
            oss << "TDMS " << parameter_name
                << " changes during waveform acquisition at file offset "
                << event.file_offset << " (" << expected << " -> "
                << event.value << ")";
            throw std::runtime_error(oss.str());
        }
    }
}

std::optional<double>
infer_sample_rate_from_timestamps(const std::vector<Timestamp> &timestamps) {
    if (timestamps.size() < 2) {
        return std::nullopt;
    }

    const long double first = timestamp_unix_seconds(timestamps.front());
    const long double last = timestamp_unix_seconds(timestamps.back());
    const long double duration = last - first;
    if (!(duration > 0.0L) || !std::isfinite(static_cast<double>(duration))) {
        return std::nullopt;
    }

    const long double fs =
        static_cast<long double>(timestamps.size() - 1) / duration;
    if (!(fs > 0.0L) || !std::isfinite(static_cast<double>(fs))) {
        return std::nullopt;
    }
    return static_cast<double>(fs);
}

double unit_multiplier(AccelerationUnit unit) {
    switch (unit) {
        case AccelerationUnit::MeterPerSecondSquared:
            return 1.0;
        case AccelerationUnit::CentimeterPerSecondSquared:
            return 100.0;
    }
    throw std::runtime_error("unsupported acceleration unit");
}

} // namespace

const char *acceleration_unit_name(AccelerationUnit unit) {
    switch (unit) {
        case AccelerationUnit::MeterPerSecondSquared:
            return "m/s^2";
        case AccelerationUnit::CentimeterPerSecondSquared:
            return "cm/s^2";
    }
    return "unknown";
}

Dataset load_dataset(const std::string &input_path,
                     const LoadOptions &options) {
    Reader reader(input_path);
    Segment segment;
    Dataset dataset;
    bool saw_waveform = false;

    while (reader.next(segment)) {
        for (const auto &object : segment.objects) {
            if (object.path == kRootPath) {
                const auto it = object.properties.find("name");
                if (it != object.properties.end()
                    && it->second.data_type == DataType::String) {
                    dataset.name = it->second.string_value;
                }
            }
        }

        for (const auto &raw : segment.raw_objects) {
            if (raw.path == kSampleRatePath) {
                auto values = decode_numeric(raw);
                for (const double value : values) {
                    dataset.sample_rate_history.push_back(value);
                    dataset.sample_rate_events.push_back(
                        {segment.file_offset, value});
                }
            } else if (raw.path == kSensitivityPath) {
                auto values = decode_numeric(raw);
                for (const double value : values) {
                    dataset.sensitivity_history.push_back(value);
                    dataset.sensitivity_events.push_back(
                        {segment.file_offset, value});
                }
            } else if (raw.path == kNorthPath) {
                update_waveform_range(dataset, segment.file_offset);
                saw_waveform = true;
                append(dataset.north_counts, decode_int32(raw));
            } else if (raw.path == kEastPath) {
                update_waveform_range(dataset, segment.file_offset);
                saw_waveform = true;
                append(dataset.east_counts, decode_int32(raw));
            } else if (raw.path == kVerticalPath) {
                update_waveform_range(dataset, segment.file_offset);
                saw_waveform = true;
                append(dataset.vertical_counts, decode_int32(raw));
            } else if (raw.path == kTimePath && options.keep_timestamps) {
                append(dataset.timestamps, decode_timestamp(raw));
            }
        }
    }

    if (!saw_waveform || dataset.north_counts.empty()
        || dataset.east_counts.empty() || dataset.vertical_counts.empty()) {
        throw std::runtime_error("TDMS file does not contain all required "
                                 "Data/N, Data/E and Data/Z channels");
    }
    const auto samples = dataset.north_counts.size();
    if (dataset.east_counts.size() != samples
        || dataset.vertical_counts.size() != samples) {
        throw std::runtime_error(
            "TDMS N/E/Z channels have different sample counts");
    }
    if (options.keep_timestamps && dataset.timestamps.size() != samples) {
        throw std::runtime_error(
            "TDMS Time channel sample count does not match N/E/Z");
    }

    const auto active_fs = value_active_before(dataset.sample_rate_events,
                                               dataset.first_waveform_offset);
    if (active_fs.has_value() && *active_fs > 0.0
        && std::isfinite(*active_fs)) {
        dataset.sample_rate_hz = *active_fs;
    } else if (options.keep_timestamps) {
        const auto inferred =
            infer_sample_rate_from_timestamps(dataset.timestamps);
        if (!inferred.has_value()) {
            throw std::runtime_error(
                "TDMS sampling rate is missing or invalid and cannot be "
                "inferred from Time channel");
        }
        dataset.sample_rate_hz = *inferred;
    } else if (dataset.sample_rate_events.empty()) {
        throw std::runtime_error(
            "TDMS file does not contain Parmt/SRate and timestamp inference "
            "is disabled");
    } else {
        throw std::runtime_error(
            "TDMS has no valid Parmt/SRate value before waveform data starts");
    }
    if (!(dataset.sample_rate_hz > 0.0)
        || !std::isfinite(dataset.sample_rate_hz)) {
        throw std::runtime_error("invalid TDMS sampling rate");
    }
    if (active_fs.has_value() && *active_fs > 0.0
        && std::isfinite(*active_fs)) {
        verify_no_parameter_change_during_waveform(
            dataset.sample_rate_events,
            dataset.first_waveform_offset,
            dataset.last_waveform_offset,
            dataset.sample_rate_hz,
            "sampling rate");
    }

    if (dataset.sensitivity_events.empty() && options.require_sensitivity) {
        throw std::runtime_error("TDMS file does not contain Parmt/Sen");
    }
    switch (options.sensitivity_selection) {
        case SensitivitySelection::Acquisition: {
            const auto active = value_active_before(
                dataset.sensitivity_events, dataset.first_waveform_offset);
            if (!active.has_value()) {
                if (options.require_sensitivity) {
                    throw std::runtime_error(
                        "TDMS has no Parmt/Sen value before waveform data "
                        "starts");
                }
                dataset.selected_sensitivity_raw = 0.0;
                break;
            }
            dataset.selected_sensitivity_raw = *active;
            if (options.require_sensitivity) {
                verify_no_parameter_change_during_waveform(
                    dataset.sensitivity_events,
                    dataset.first_waveform_offset,
                    dataset.last_waveform_offset,
                    dataset.selected_sensitivity_raw,
                    "sensitivity");
            }
            break;
        }
        case SensitivitySelection::First:
            if (dataset.sensitivity_history.empty()) {
                if (options.require_sensitivity) {
                    throw std::runtime_error(
                        "TDMS file does not contain Parmt/Sen");
                }
                dataset.selected_sensitivity_raw = 0.0;
            } else {
                dataset.selected_sensitivity_raw =
                    dataset.sensitivity_history.front();
            }
            break;
        case SensitivitySelection::Last:
            if (dataset.sensitivity_history.empty()) {
                if (options.require_sensitivity) {
                    throw std::runtime_error(
                        "TDMS file does not contain Parmt/Sen");
                }
                dataset.selected_sensitivity_raw = 0.0;
            } else {
                dataset.selected_sensitivity_raw =
                    dataset.sensitivity_history.back();
            }
            break;
        case SensitivitySelection::Explicit:
            dataset.selected_sensitivity_raw = options.explicit_sensitivity;
            break;
    }
    if (options.require_sensitivity
        && (!(dataset.selected_sensitivity_raw > 0.0)
            || !std::isfinite(dataset.selected_sensitivity_raw))) {
        throw std::runtime_error("selected TDMS sensitivity is invalid");
    }

    if (options.keep_timestamps) {
        if (options.verify_time_axis && dataset.timestamps.size() >= 2) {
            const long double expected_dt =
                1.0L / static_cast<long double>(dataset.sample_rate_hz);
            constexpr long double tolerance = 1e-6L;
            for (std::size_t i = 1; i < dataset.timestamps.size(); ++i) {
                const auto dt =
                    timestamp_unix_seconds(dataset.timestamps[i])
                    - timestamp_unix_seconds(dataset.timestamps[i - 1]);
                if (std::fabs(dt - expected_dt) > tolerance) {
                    std::ostringstream oss;
                    oss << "TDMS time discontinuity at sample " << i
                        << ": dt=" << static_cast<double>(dt) << " s, expected "
                        << static_cast<double>(expected_dt) << " s";
                    throw std::runtime_error(oss.str());
                }
            }
        }
    }

    return dataset;
}

void export_text(const Dataset &dataset,
                 const std::string &output_path,
                 const TextExportOptions &options) {
    if (options.precision < 1 || options.precision > 17) {
        throw std::runtime_error("text precision must be between 1 and 17");
    }
    const std::size_t rows = dataset.north_counts.size();
    if (dataset.east_counts.size() != rows
        || dataset.vertical_counts.size() != rows) {
        throw std::runtime_error(
            "cannot export TDMS dataset with mismatched N/E/Z lengths");
    }
    if (!options.output_counts
        && (!(dataset.selected_sensitivity_raw > 0.0)
            || !std::isfinite(dataset.selected_sensitivity_raw))) {
        throw std::runtime_error(
            "invalid sensitivity for TDMS physical conversion");
    }
    if (!(options.sensitivity_storage_scale > 0.0)
        || !std::isfinite(options.sensitivity_storage_scale)) {
        throw std::runtime_error(
            "TDMS sensitivity storage scale must be positive and finite");
    }
    if (!std::isfinite(options.post_scale)) {
        throw std::runtime_error("TDMS post scale must be finite");
    }

    std::ofstream out(output_path);
    if (!out) {
        throw std::runtime_error("cannot create output text file: "
                                 + output_path);
    }
    out << std::setprecision(options.precision) << std::scientific;
    if (options.write_header) {
        out << "N" << options.delimiter << "E" << options.delimiter << "Z\n";
    }

    const double actual_sensitivity =
        dataset.selected_sensitivity_raw / options.sensitivity_storage_scale;
    const double output_unit_scale = unit_multiplier(options.output_unit);

    for (std::size_t i = 0; i < rows; ++i) {
        if (options.output_counts) {
            out << dataset.north_counts[i] << options.delimiter
                << dataset.east_counts[i] << options.delimiter
                << dataset.vertical_counts[i] << '\n';
        } else {
            const auto convert = [&](std::int32_t count) {
                const double value_mps2 =
                    static_cast<double>(count) / actual_sensitivity;
                return value_mps2 * output_unit_scale * options.post_scale;
            };
            out << convert(dataset.north_counts[i]) << options.delimiter
                << convert(dataset.east_counts[i]) << options.delimiter
                << convert(dataset.vertical_counts[i]) << '\n';
        }
    }
    if (!out) {
        throw std::runtime_error("failed while writing TDMS text output");
    }

    if (options.write_metadata) {
        std::ofstream meta(metadata_path_for(output_path));
        if (!meta) {
            throw std::runtime_error("cannot create TDMS metadata sidecar");
        }
        meta << std::setprecision(17);
        meta << "source_format=TDMS\n";
        meta << "name=" << dataset.name << "\n";
        meta << "sample_rate_hz=" << dataset.sample_rate_hz << "\n";
        meta << "sample_count=" << rows << "\n";
        meta << "column_count=3\n";
        meta << "column_1=N\ncolumn_2=E\ncolumn_3=Z\n";
        if (!dataset.timestamps.empty()) {
            meta << "start_time_utc="
                 << format_timestamp_utc(dataset.timestamps.front(), 6) << "\n";
            meta << "end_time_utc="
                 << format_timestamp_utc(dataset.timestamps.back(), 6) << "\n";
        }
        meta << "sensitivity_values_raw=";
        for (std::size_t i = 0; i < dataset.sensitivity_history.size(); ++i) {
            if (i)
                meta << ',';
            meta << dataset.sensitivity_history[i];
        }
        meta << "\nselected_sensitivity_raw="
             << dataset.selected_sensitivity_raw << "\n";
        meta << "sensitivity_storage_scale="
             << options.sensitivity_storage_scale << "\n";
        meta << "actual_sensitivity_count_per_mps2=" << actual_sensitivity
             << "\n";
        if (options.output_counts) {
            meta << "output=raw_count\nunit=count\nconversion=none\n";
        } else {
            meta << "output=acceleration\n";
            meta << "unit=" << acceleration_unit_name(options.output_unit)
                 << "\n";
            meta << "conversion=count/(Sen_raw/"
                    "sensitivity_storage_scale)*unit_scale*post_scale\n";
            meta << "unit_scale=" << output_unit_scale << "\n";
            meta << "post_scale=" << options.post_scale << "\n";
            meta << "calibration_note=reference data verify Sen_raw = actual "
                    "sensitivity x 100; acquisition-time Sen is used by "
                    "default.\n";
        }
    }
}

} // namespace qrest_data::tools::tdms
