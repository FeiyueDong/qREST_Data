#pragma once

#include "tdms_reader.hpp"

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace qrest_data::tools::tdms {

enum class SensitivitySelection {
    // Select the sensitivity that is active immediately before waveform data
    // starts. This is the safest default for files that write a trailing
    // configuration value after all waveform segments have finished.
    Acquisition,
    First,
    Last,
    Explicit,
};

enum class AccelerationUnit {
    MeterPerSecondSquared,
    CentimeterPerSecondSquared,
};

struct ParameterEvent {
    std::uint64_t file_offset{};
    double value{};
};

struct LoadOptions {
    SensitivitySelection sensitivity_selection{
        SensitivitySelection::Acquisition};
    double explicit_sensitivity{};
    bool verify_time_axis{true};
    bool keep_timestamps{true};
};

struct Dataset {
    std::string name;
    double sample_rate_hz{};

    std::vector<double> sample_rate_history;
    std::vector<ParameterEvent> sample_rate_events;
    std::vector<double> sensitivity_history;
    std::vector<ParameterEvent> sensitivity_events;

    // Raw TDMS Parmt/Sen value. For the supplied monitoring format this value
    // is verified against reference data to be actual sensitivity x 100.
    double selected_sensitivity_raw{};

    std::uint64_t first_waveform_offset{};
    std::uint64_t last_waveform_offset{};

    std::vector<std::int32_t> north_counts;
    std::vector<std::int32_t> east_counts;
    std::vector<std::int32_t> vertical_counts;
    std::vector<Timestamp> timestamps;
};

struct TextExportOptions {
    char delimiter{'\t'};
    int precision{12};
    bool write_header{false};
    bool write_metadata{true};
    bool output_counts{false};

    // Verified calibration convention for this monitoring TDMS format:
    //   Sen_raw = sensitivity[count/(m/s^2)] * 100
    double sensitivity_storage_scale{100.0};

    // Reference converted files use cm/s^2, so keep that as the default.
    // Use --unit m/s2 when SI output is preferred.
    AccelerationUnit output_unit{AccelerationUnit::CentimeterPerSecondSquared};

    // Optional additional multiplier after calibrated unit conversion.
    double post_scale{1.0};
};

Dataset load_dataset(const std::string &input_path,
                     const LoadOptions &options = {});

void export_text(const Dataset &dataset,
                 const std::string &output_path,
                 const TextExportOptions &options = {});

const char *acceleration_unit_name(AccelerationUnit unit);

} // namespace qrest_data::tools::tdms
