#ifndef QREST_DATA_TOOLS_EXTERNAL_IMPORT_HPP
#define QREST_DATA_TOOLS_EXTERNAL_IMPORT_HPP

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "metadata.hpp"
#include "validation.hpp"

namespace qrest_data::tools {

struct ExternalDataset {
    std::string source_format;
    std::size_t channel_count{};
    std::size_t sample_count{};
    double sample_rate_hz{};
    std::vector<std::string> channel_labels;
    std::vector<double> channel_sequential_data;
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

struct MseedImportOptions {
    std::size_t group_index{};
    bool include_dimensionless{false};
    bool verify_time_continuity{true};
};

ExternalDataset load_tdms_dataset(const std::string &input_path,
                                  const TdmsImportOptions &options = {});

ExternalDataset load_mseed_dataset(const std::string &input_path,
                                   const MseedImportOptions &options = {});

ExternalDataset load_hdf5_dataset(const std::string &input_path,
                                  Metadata *metadata = nullptr);

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
