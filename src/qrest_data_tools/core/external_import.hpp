/**
**            qREST - Quick Response Evaluation for Safety Tagging
**     Institute of Engineering Mechanics, China Earthquake Administration
**                 Copyright 2024 - 2026 QLab, Dong Feiyue
**                          All Rights Reserved.
** Project: qREST
** File: /src/qrest_data_tools/core/external_import.hpp
** File Created: not recorded in existing source
** Author: not recorded in existing source
** Last Modified: Sunday, 18 October 2026
** Modified By: Codex
*/
#ifndef QREST_DATA_TOOLS_EXTERNAL_IMPORT_HPP
#define QREST_DATA_TOOLS_EXTERNAL_IMPORT_HPP

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include <qrest_data/metadata.hpp>
#include "validation.hpp"

namespace qrest_data::tools {

struct ExternalChannelInfo {
    std::string source_id;
    std::string source_label;
    std::string
        axis_hint; // North / East / Vertical / Unknown; never structural X/Y
    std::size_t source_order{};
    std::int64_t start_time_us{};
    std::uint64_t record_count{};
    std::uint64_t missing_samples{};
    std::optional<double> sensitivity; // count/SI-unit
    std::string sensitivity_origin;
    std::string source_unit;
};

struct ExternalDataset {
    std::string source_format;
    std::size_t channel_count{};
    std::size_t sample_count{};
    double sample_rate_hz{};
    std::optional<std::uint64_t> start_time_ms;
    std::vector<std::string> channel_labels;
    // Empty when the source does not declare a known representation.
    std::vector<std::string> channel_measurands;
    std::string distance_unit;
    std::vector<double> channel_sequential_data;
    std::vector<ExternalChannelInfo> channels;
    std::string
        representation; // RawCounts / Physical (empty for legacy adapters)
    std::string output_unit;
    std::string time_basis;
    std::optional<std::int64_t> start_time_us;
    std::vector<std::string> diagnostics;
};

struct ExternalChannelMapping {
    std::size_t source_channel{};
    std::size_t target_channel{};
};

struct TdmsImportOptions {
    enum class Unit {
        MeterPerSecondSquared,
        CentimeterPerSecondSquared,
    };

    enum class SensitivitySelection {
        Acquisition,
        First,
        Last,
        Explicit,
    };

    Unit output_unit{Unit::CentimeterPerSecondSquared};
    SensitivitySelection sensitivity_selection{
        SensitivitySelection::Acquisition};
    double explicit_sensitivity{};
    double sensitivity_storage_scale{100.0};
    double post_scale{1.0};
    bool output_counts{false};
    bool verify_time_axis{true};
};

enum class MotionUnit { SI, Centimeter };

struct MseedImportOptions {
    enum class GapPolicy {
        FillNaN,
        Error,
        Ignore,
    };

    std::size_t group_index{};
    bool include_dimensionless{false};
    GapPolicy gap_policy{GapPolicy::FillNaN};
    MotionUnit output_unit{MotionUnit::SI}; // preserves old MiniSEED scale
    bool output_counts{false};
    bool require_physical{false};
    std::optional<double> sensitivity; // count per SI-unit
    std::string quantity; // Acceleration / Velocity / Displacement; required
                          // with sensitivity
};

struct SitDatImportOptions : MseedImportOptions {
    SitDatImportOptions() { output_unit = MotionUnit::Centimeter; }
};

struct SitTxtImportOptions {
    MotionUnit output_unit{MotionUnit::Centimeter};
    std::string timezone{"+08:00"};
};

ExternalDataset load_tdms_dataset(const std::string &input_path,
                                  const TdmsImportOptions &options = {});

ExternalDataset load_tdms_external_data(const std::string &input_path,
                                        const TdmsImportOptions &options = {});

ExternalDataset load_tdms_collection(const std::string &input_path,
                                     const Metadata &metadata,
                                     const TdmsImportOptions &options = {});

ExternalDataset load_mseed_dataset(const std::string &input_path,
                                   const MseedImportOptions &options = {});

ExternalDataset
load_mseed_external_data(const std::string &input_path,
                         const MseedImportOptions &options = {});

ExternalDataset load_mseed_collection(const std::string &input_path,
                                      const Metadata &metadata,
                                      const MseedImportOptions &options = {});

ExternalDataset load_sit_dat_dataset(const std::string &path,
                                     const SitDatImportOptions &options = {});
ExternalDataset load_sit_txt_dataset(const std::string &path,
                                     const SitTxtImportOptions &options = {});

// Summary is also returned by the shared C API. No file is written.
std::string external_dataset_summary_json(const ExternalDataset &dataset);
std::string external_axis_hint(const std::string &label);

// Shared CLI/SDK option schema; rejects unknown or inapplicable options.
ExternalDataset read_external_dataset(const std::string &path,
                                      const std::string &format,
                                      const std::string &options_json = "{}");

ExternalDataset load_hdf5_dataset(const std::string &input_path,
                                  Metadata *metadata = nullptr);

std::vector<ExternalChannelMapping>
make_sequential_channel_mapping(const ExternalDataset &dataset,
                                const Metadata &metadata);

ValidationReport validate_external_channel_mapping(
    const ExternalDataset &dataset,
    const Metadata &metadata,
    const std::vector<ExternalChannelMapping> &mapping);

ExternalDataset apply_external_channel_mapping(
    const ExternalDataset &dataset,
    const Metadata &metadata,
    const std::vector<ExternalChannelMapping> &mapping);

void write_hdf5_dataset(const std::string &output_path,
                        const Metadata &metadata,
                        const std::vector<double> &channel_sequential_data);

ValidationReport
validate_external_dataset_compatibility(const ExternalDataset &dataset,
                                        const Metadata &metadata);

void require_external_dataset_compatibility(const ExternalDataset &dataset,
                                            const Metadata &metadata);

} // namespace qrest_data::tools

#endif // QREST_DATA_TOOLS_EXTERNAL_IMPORT_HPP
