#pragma once

#include "modified_mseed.hpp"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace qrest_data::tools::mseed {

struct DataGap {
    // Start time of the first record after the missing interval.
    BTime next_record_start{};
    std::uint64_t missing_samples{};
    double duration_seconds{};
};

// One physical channel reconstructed from many records. Missing samples are
// represented by IEEE quiet NaN when GapPolicy::FillNaN is used.
struct ChannelSeries {
    std::string network;
    std::string station;
    std::string location;
    std::string channel;
    std::string channel_order;
    std::string source_format;
    std::size_t source_order{};
    bool raw_counts{false};
    std::string sensitivity_origin;
    double actual_sensitivity{};

    std::uint32_t start_time_resolution_us{100};
    // Common grid-phase window inferred from all records, relative to the
    // encoded first timestamp. This is an integrity bound, not a clock
    // correction.
    long double start_phase_lower_us{-50};
    long double start_phase_upper_us{50};
    std::int64_t last_record_time_us{};
    std::uint32_t last_record_time_resolution_us{100};
    std::int64_t start_time_us{};
    BTime start_time{};
    BTime last_record_time{};
    double sample_rate_hz{};
    Dimension dimension{Dimension::Dimensionless};
    std::uint32_t sensitivity{};
    std::uint64_t record_count{};
    std::uint64_t valid_sample_count{};
    std::uint64_t missing_sample_count{};
    std::uint64_t leading_missing_sample_count{};
    std::uint64_t trailing_missing_sample_count{};
    std::vector<DataGap> gaps;

    // The stored sensitivity field is actual sensitivity * 100. Values are
    // converted using count * 100 / sensitivity. For displacement, velocity
    // and acceleration the output is SI: m, m/s and m/s^2 respectively.
    std::vector<double> values;
};

// Channels that can be written as columns of the same text matrix. With
// GapPolicy::FillNaN, channels are aligned onto the same sampling grid and
// missing leading/internal/trailing samples are NaN.
struct ChannelGroup {
    std::string network;
    std::string station;
    std::string location;
    double sample_rate_hz{};
    Dimension dimension{Dimension::Dimensionless};
    std::int64_t start_time_us{};
    BTime start_time{};
    std::vector<ChannelSeries> channels;

    std::size_t sample_count() const noexcept;
};

enum class GapPolicy {
    // Preserve the sampling grid and insert NaN for each missing sample.
    // Positive gaps must align with an integer number of sample intervals;
    // overlaps and non-grid timing jumps remain errors.
    FillNaN,

    // Original strict behavior: any inter-record time discontinuity is an
    // error.
    Error,

    // Compatibility/debug behavior: concatenate records without checking time.
    // This destroys the true time axis across a gap and is not recommended for
    // analysis data.
    Ignore,
};

struct LoadOptions {
    // Generic count * 100 / stored-sensitivity conversion is used for the
    // motion dimensions encoded by this protocol. Dimensionless SOH channels
    // such as VEP/VPB have channel-specific engineering units, so they are
    // excluded by default rather than being mislabeled as SI waveform data.
    bool include_dimensionless{false};

    // Missing-record handling. FillNaN is the default so a partially missing
    // stream can still be loaded without shifting later samples in time.
    GapPolicy gap_policy{GapPolicy::FillNaN};
    bool output_counts{false};
    std::optional<double>
        sensitivity; // count per SI unit; used only without native calibration
    Dimension quantity{Dimension::Unknown};
};

struct TextExportOptions {
    char delimiter{'\t'};
    int precision{12};

    // Keep the data file strictly numeric. When false (default), the file has
    // exactly sample_count() lines. Metadata/column names are written to a
    // separate <output>.meta.txt file.
    bool write_header{false};
    bool write_metadata{true};
};

// Read all selected records, reconstruct channels, convert counts to physical
// values, repair missing samples according to LoadOptions::gap_policy, and
// organize synchronized channels into column groups.
std::vector<ChannelGroup> load_channel_groups(const std::string &input_path,
                                              const LoadOptions &options = {});

std::vector<ChannelGroup>
assemble_channel_groups(std::vector<Record> records,
                        const LoadOptions &options = {});

// Write one synchronized group as a plain-text matrix. One channel per column,
// one sample instant per row. Column order preserves first appearance in the
// source. Missing samples are written as NaN. Counts without calibration remain
// counts in both the values and metadata sidecar.
void export_group_text(const ChannelGroup &group,
                       const std::string &output_path,
                       const TextExportOptions &options = {});

// Return the same channel order used by export_group_text.
std::vector<std::size_t> ordered_channel_indices(const ChannelGroup &group);

std::string physical_unit_name(Dimension d);

} // namespace qrest_data::tools::mseed
