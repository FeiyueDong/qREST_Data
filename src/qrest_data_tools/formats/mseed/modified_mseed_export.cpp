#include "modified_mseed_export.hpp"

#include <algorithm>
#include <cmath>
#include <fstream>
#include <iomanip>
#include <limits>
#include <map>
#include <sstream>
#include <stdexcept>
#include <tuple>
#include <utility>

namespace qrest_data::tools::mseed {
namespace {

bool almost_equal(double a, double b) {
    const double scale = std::max({1.0, std::abs(a), std::abs(b)});
    return std::abs(a - b) <= 1e-12 * scale;
}

std::string stream_name(const Header &h) {
    return h.network + "." + h.station + "." + h.location + "." + h.channel;
}

std::string stream_name(const ChannelSeries &s) {
    return s.network + "." + s.station + "." + s.location + "." + s.channel;
}

// All calculations use relative microseconds, avoiding cancellation of Unix
// epoch seconds (in particular on MSVC, where long double has double
// precision). A timestamp pair contributes half of each encoding step to its
// error budget. This checks a unique integer sample offset; it never rounds an
// arbitrary clock jump onto the grid or grows uncertainty with the number of
// records.
std::int64_t time_offset_to_samples(long double delta_us,
                                    double sample_rate,
                                    std::uint32_t reference_resolution_us,
                                    std::uint32_t current_resolution_us,
                                    const std::string &context,
                                    long double elapsed_us = 0) {
    if (!(sample_rate > 0.0) || !std::isfinite(sample_rate))
        throw std::runtime_error("invalid sample rate while checking "
                                 + context);
    const long double interval_us = 1000000.0L / sample_rate;
    const long double budget_us =
        (static_cast<long double>(reference_resolution_us)
         + current_resolution_us)
        / 2;
    const long double arithmetic_us =
        32 * std::numeric_limits<long double>::epsilon()
        * std::max(
            {1.0L, std::fabs(delta_us), std::fabs(elapsed_us), interval_us});
    const long double tolerance_us = budget_us + arithmetic_us;
    const auto detail = [&]() {
        std::ostringstream detail;
        detail << std::setprecision(16) << context << ": delta_us=" << delta_us
               << ", interval_us=" << interval_us
               << ", tolerance_us=" << tolerance_us
               << ", timestamp_resolutions_us=" << reference_resolution_us
               << '/' << current_resolution_us;
        return detail.str();
    };
    if (!reference_resolution_us || !current_resolution_us
        || !(tolerance_us < interval_us / 2))
        throw std::runtime_error(
            "insufficient timing precision for unambiguous sampling grid in "
            + detail());
    const long double rounded = std::round(delta_us / interval_us);
    // Exclusive powers-of-two bounds also work when long double == double.
    if (!std::isfinite(rounded) || rounded >= std::ldexp(1.0L, 63)
        || rounded < -std::ldexp(1.0L, 63))
        throw std::runtime_error("time discontinuity is too large in "
                                 + detail());
    const long double residual_us = std::fabs(delta_us - rounded * interval_us);
    if (residual_us > tolerance_us) {
        std::ostringstream extra;
        extra << detail() << ", nearest_sample_offset=" << rounded
              << ", residual_us=" << residual_us;
        throw std::runtime_error(
            "time discontinuity is not aligned to the sampling grid in "
            + extra.str());
    }
    return static_cast<std::int64_t>(rounded);
}

std::string record_time_context(const Header &h,
                                const ChannelSeries &series,
                                long double expected_elapsed_us) {
    std::ostringstream text;
    text << "stream " << stream_name(h)
         << ", previous_record_start_us=" << series.last_record_time_us
         << ", current_record_start_us=" << h.start_time_us
         << ", grid_start_us=" << series.start_time_us
         << ", expected_next_start_us=" << std::fixed << std::setprecision(3)
         << static_cast<long double>(series.start_time_us)
                + expected_elapsed_us;
    return text.str();
}

struct ChannelKey {
    std::string network;
    std::string station;
    std::string location;
    std::string channel;

    bool operator<(const ChannelKey &rhs) const {
        return std::tie(network, station, location, channel)
               < std::tie(rhs.network, rhs.station, rhs.location, rhs.channel);
    }
};

struct Accumulator {
    ChannelSeries series;
    bool initialized{false};
    std::size_t previous_record_samples{};
};

struct GroupKey {
    std::string network;
    std::string station;
    std::string location;
    Dimension dimension{Dimension::Dimensionless};
    double sample_rate{};
    // Used only by GapPolicy::Ignore to preserve the original grouping
    // behavior when streams have different starts/lengths.
    std::string unchecked_start_time;
    std::size_t unchecked_sample_count{};

    bool operator<(const GroupKey &rhs) const {
        return std::tie(network,
                        station,
                        location,
                        dimension,
                        sample_rate,
                        unchecked_start_time,
                        unchecked_sample_count)
               < std::tie(rhs.network,
                          rhs.station,
                          rhs.location,
                          rhs.dimension,
                          rhs.sample_rate,
                          rhs.unchecked_start_time,
                          rhs.unchecked_sample_count);
    }
};

void prepend_missing(ChannelSeries &series, std::uint64_t count) {
    if (count == 0) {
        return;
    }
    if (count
        > static_cast<std::uint64_t>(std::numeric_limits<std::size_t>::max())) {
        throw std::runtime_error("leading gap is too large in stream "
                                 + stream_name(series));
    }
    const double nan = std::numeric_limits<double>::quiet_NaN();
    series.values.insert(
        series.values.begin(), static_cast<std::size_t>(count), nan);
    series.leading_missing_sample_count += count;
    series.missing_sample_count += count;
}

void append_missing(ChannelSeries &series, std::uint64_t count) {
    if (count == 0) {
        return;
    }
    if (count
        > static_cast<std::uint64_t>(std::numeric_limits<std::size_t>::max())) {
        throw std::runtime_error("gap is too large in stream "
                                 + stream_name(series));
    }
    const double nan = std::numeric_limits<double>::quiet_NaN();
    series.values.insert(
        series.values.end(), static_cast<std::size_t>(count), nan);
    series.missing_sample_count += count;
}

void align_group_channels(ChannelGroup &group, GapPolicy policy) {
    if (group.channels.empty())
        return;
    // Select an integer timestamp reference, then work only in offsets from it.
    const auto earliest =
        std::min_element(group.channels.begin(),
                         group.channels.end(),
                         [](const auto &a, const auto &b) {
                             return a.start_time_us < b.start_time_us;
                         });
    const auto earliest_us = earliest->start_time_us;
    const auto earliest_resolution = earliest->start_time_resolution_us;
    auto phase_lower_us = earliest->start_phase_lower_us;
    auto phase_upper_us = earliest->start_phase_upper_us;
    group.start_time_us = earliest_us;
    group.start_time = earliest->start_time;
    std::size_t latest_index = 0;
    long double group_end_us = 0;
    for (std::size_t i = 0; i < group.channels.size(); ++i) {
        const auto &ch = group.channels[i];
        const auto end =
            static_cast<long double>(ch.start_time_us - earliest_us)
            + ch.values.size() * 1000000.0L / ch.sample_rate_hz;
        if (i == 0 || end > group_end_us) {
            group_end_us = end;
            latest_index = i;
        }
    }
    const auto end_resolution =
        group.channels[latest_index].start_time_resolution_us;
    for (auto &ch : group.channels) {
        const long double start_us =
            static_cast<long double>(ch.start_time_us - earliest_us);
        const long double end_us =
            start_us + ch.values.size() * 1000000.0L / ch.sample_rate_hz;
        const auto lead = time_offset_to_samples(start_us,
                                                 ch.sample_rate_hz,
                                                 earliest_resolution,
                                                 ch.start_time_resolution_us,
                                                 "leading alignment of stream "
                                                     + stream_name(ch));
        const auto trail = time_offset_to_samples(
            group_end_us - end_us,
            ch.sample_rate_hz,
            end_resolution,
            ch.start_time_resolution_us,
            "trailing alignment of stream " + stream_name(ch),
            group_end_us);
        const auto residual_us =
            start_us - lead * 1000000.0L / ch.sample_rate_hz;
        phase_lower_us =
            std::max(phase_lower_us, residual_us + ch.start_phase_lower_us);
        phase_upper_us =
            std::min(phase_upper_us, residual_us + ch.start_phase_upper_us);
        const auto arithmetic_us = 32
                                   * std::numeric_limits<long double>::epsilon()
                                   * std::max(1.0L, std::fabs(group_end_us));
        if (phase_lower_us > phase_upper_us + arithmetic_us)
            throw std::runtime_error("inconsistent cross-channel timestamp "
                                     "quantization phase in stream "
                                     + stream_name(ch));
        if (lead < 0 || trail < 0)
            throw std::runtime_error("invalid channel alignment in stream "
                                     + stream_name(ch));
        if (policy == GapPolicy::Error && (lead || trail))
            throw std::runtime_error(
                "channel coverage differs under gap-policy=error in stream "
                + stream_name(ch));
        prepend_missing(ch, static_cast<std::uint64_t>(lead));
        append_missing(ch, static_cast<std::uint64_t>(trail));
        ch.trailing_missing_sample_count += static_cast<std::uint64_t>(trail);
        ch.start_time_us = earliest_us;
        ch.start_time = group.start_time;
        ch.start_time_resolution_us = earliest_resolution;
    }
    if (phase_lower_us > phase_upper_us)
        phase_lower_us = phase_upper_us = (phase_lower_us + phase_upper_us) / 2;
    for (auto &ch : group.channels) {
        ch.start_phase_lower_us = phase_lower_us;
        ch.start_phase_upper_us = phase_upper_us;
    }
    const auto expected = group.channels.front().values.size();
    for (const auto &ch : group.channels)
        if (ch.values.size() != expected)
            throw std::runtime_error(
                "failed to align channel lengths in stream " + stream_name(ch));
}

} // namespace

std::size_t ChannelGroup::sample_count() const noexcept {
    return channels.empty() ? 0u : channels.front().values.size();
}

std::vector<std::size_t> ordered_channel_indices(const ChannelGroup &group) {
    std::vector<std::size_t> result;
    result.reserve(group.channels.size());

    for (std::size_t i = 0; i < group.channels.size(); ++i)
        result.push_back(i);
    std::stable_sort(
        result.begin(), result.end(), [&](std::size_t a, std::size_t b) {
            return group.channels[a].source_order
                   < group.channels[b].source_order;
        });
    return result;
}

namespace {

std::string metadata_path_for(const std::string &output_path) {
    return output_path + ".meta.txt";
}

} // namespace

std::vector<ChannelGroup> load_channel_groups(const std::string &input_path,
                                              const LoadOptions &options) {
    Reader reader(input_path);
    Record record;
    std::vector<Record> records;
    while (reader.next(record))
        records.push_back(std::move(record));
    return assemble_channel_groups(std::move(records), options);
}

std::vector<ChannelGroup> assemble_channel_groups(std::vector<Record> records,
                                                  const LoadOptions &options) {
    if (options.sensitivity
        && (!std::isfinite(*options.sensitivity) || *options.sensitivity <= 0))
        throw std::runtime_error(
            "sensitivity must be positive and finite (count/SI unit)");
    if (options.sensitivity && options.quantity == Dimension::Unknown)
        throw std::runtime_error(
            "explicit sensitivity requires an explicit physical quantity");
    if (options.output_counts && options.sensitivity)
        throw std::runtime_error(
            "counts and physical sensitivity options are mutually exclusive");
    std::map<ChannelKey, std::size_t> source_order;
    for (const auto &record : records) {
        const auto &h = record.header;
        source_order.try_emplace({h.network, h.station, h.location, h.channel},
                                 source_order.size());
    }
    std::stable_sort(
        records.begin(), records.end(), [](const auto &a, const auto &b) {
            return a.header.start_time_us < b.header.start_time_us;
        });
    std::map<ChannelKey, Accumulator> channels;
    for (const auto &record : records) {
        const Header &h = record.header;
        if (!h.sample_count)
            continue;
        if (h.dimension == Dimension::Dimensionless
            && !options.include_dimensionless)
            continue;
        const double sensitivity = h.sensitivity
                                       ? h.sensitivity / 100.0
                                       : options.sensitivity.value_or(0.0);
        const Dimension quantity =
            h.has_extension ? h.dimension : options.quantity;
        if (h.sensitivity && options.sensitivity
            && !almost_equal(sensitivity, *options.sensitivity))
            throw std::runtime_error(
                "explicit sensitivity conflicts with native sensitivity in "
                + stream_name(h));
        if (h.has_extension && options.quantity != Dimension::Unknown
            && h.dimension != options.quantity)
            throw std::runtime_error(
                "explicit quantity conflicts with native quantity in "
                + stream_name(h));
        const bool raw = options.output_counts || sensitivity == 0
                         || quantity == Dimension::Unknown;
        const double fs = sample_rate_hz(h);
        if (!(fs > 0.0) || !std::isfinite(fs)) {
            throw std::runtime_error("invalid sample rate for selected stream "
                                     + stream_name(h));
        }

        ChannelKey key{h.network, h.station, h.location, h.channel};
        auto &acc = channels[key];

        if (!acc.initialized) {
            acc.initialized = true;
            acc.series.network = h.network;
            acc.series.station = h.station;
            acc.series.location = h.location;
            acc.series.channel = h.channel;
            acc.series.channel_order = h.channel_order;
            acc.series.start_time = h.start_time;
            acc.series.start_time_us = h.start_time_us;
            acc.series.start_time_resolution_us = h.time_resolution_us();
            acc.series.start_phase_lower_us =
                -static_cast<long double>(h.time_resolution_us()) / 2;
            acc.series.start_phase_upper_us =
                static_cast<long double>(h.time_resolution_us()) / 2;
            acc.series.source_order = source_order.at(key);
            acc.series.source_format =
                h.standard_v2 ? "miniseed-v2" : "modified-miniseed";
            acc.series.raw_counts = raw;
            acc.series.actual_sensitivity = sensitivity;
            acc.series.sensitivity_origin =
                h.sensitivity ? "native-extension"
                              : (options.sensitivity ? "user" : "unknown");
            acc.series.last_record_time = h.start_time;
            acc.series.sample_rate_hz = fs;
            acc.series.dimension = quantity;
            acc.series.sensitivity = h.sensitivity;
        } else {
            if (!almost_equal(acc.series.sample_rate_hz, fs)) {
                throw std::runtime_error("sample rate changed within stream "
                                         + stream_name(h));
            }
            if (acc.series.dimension != quantity) {
                throw std::runtime_error("dimension changed within stream "
                                         + stream_name(h));
            }
            if (acc.series.source_format
                    != (h.standard_v2 ? "miniseed-v2" : "modified-miniseed")
                || acc.series.raw_counts != raw
                || acc.series.sensitivity != h.sensitivity) {
                throw std::runtime_error("sensitivity changed within stream "
                                         + stream_name(h));
            }
            if (!h.channel_order.empty() && !acc.series.channel_order.empty()
                && h.channel_order != acc.series.channel_order) {
                throw std::runtime_error(
                    "channel-order marker changed within stream "
                    + stream_name(h));
            }

            if (options.gap_policy != GapPolicy::Ignore) {
                const long double elapsed_us =
                    acc.series.values.size() * 1000000.0L / fs;
                const long double delta_us =
                    static_cast<long double>(h.start_time_us
                                             - acc.series.start_time_us)
                    - elapsed_us;
                const auto context =
                    record_time_context(h, acc.series, elapsed_us);
                const auto offset =
                    time_offset_to_samples(delta_us,
                                           fs,
                                           acc.series.start_time_resolution_us,
                                           h.time_resolution_us(),
                                           context,
                                           elapsed_us);
                // Check the adjacent pair too: a coarse first record must not
                // hide a later clock jump between two precise records.
                const auto previous_duration_us =
                    acc.previous_record_samples * 1000000.0L / fs;
                const auto adjacent_delta =
                    static_cast<long double>(h.start_time_us
                                             - acc.series.last_record_time_us)
                    - previous_duration_us;
                const auto adjacent_offset = time_offset_to_samples(
                    adjacent_delta,
                    fs,
                    acc.series.last_record_time_resolution_us,
                    h.time_resolution_us(),
                    context,
                    previous_duration_us);
                if (offset != adjacent_offset)
                    throw std::runtime_error(
                        "inconsistent adjacent/anchored sampling grid in "
                        + context);
                if (offset < 0)
                    throw std::runtime_error("time overlap in " + context
                                             + ", overlap_samples="
                                             + std::to_string(-offset));
                const auto residual_us = delta_us - offset * 1000000.0L / fs;
                const auto half_step =
                    static_cast<long double>(h.time_resolution_us()) / 2;
                const auto lower = std::max(acc.series.start_phase_lower_us,
                                            residual_us - half_step);
                const auto upper = std::min(acc.series.start_phase_upper_us,
                                            residual_us + half_step);
                const auto arithmetic_us =
                    32 * std::numeric_limits<long double>::epsilon()
                    * std::max(1.0L, std::fabs(elapsed_us));
                if (lower > upper + arithmetic_us) {
                    std::ostringstream detail;
                    detail << "time discontinuity: no common timestamp "
                              "quantization phase in "
                           << context << ", residual_us=" << residual_us
                           << ", interval_us=" << 1000000.0L / fs
                           << ", tolerance_us="
                           << (acc.series.start_time_resolution_us
                               + h.time_resolution_us())
                                  / 2.0L;
                    throw std::runtime_error(detail.str());
                }
                // Closed endpoints retain legitimate halfway quantization,
                // including round-to-even encoders. Collapse only roundoff.
                acc.series.start_phase_lower_us =
                    lower <= upper ? lower : (lower + upper) / 2;
                acc.series.start_phase_upper_us =
                    lower <= upper ? upper : (lower + upper) / 2;
                if (offset > 0) {
                    if (options.gap_policy == GapPolicy::Error)
                        throw std::runtime_error(
                            "time discontinuity in " + context
                            + ", missing_samples=" + std::to_string(offset));
                    append_missing(acc.series,
                                   static_cast<std::uint64_t>(offset));
                    acc.series.gaps.push_back(
                        {h.start_time,
                         static_cast<std::uint64_t>(offset),
                         double(offset) / fs});
                }
            }
        }

        for (std::int32_t count : record.samples) {
            acc.series.values.push_back(raw ? static_cast<double>(count)
                                            : static_cast<double>(count)
                                                  / sensitivity);
        }
        acc.series.valid_sample_count += record.samples.size();
        ++acc.series.record_count;
        acc.series.last_record_time = h.start_time;
        acc.series.last_record_time_us = h.start_time_us;
        acc.series.last_record_time_resolution_us = h.time_resolution_us();
        acc.previous_record_samples = record.samples.size();
    }

    if (channels.empty()) {
        throw std::runtime_error(
            options.include_dimensionless
                ? "no channels were found in the input file"
                : "no displacement/velocity/acceleration channels were found; "
                  "dimensionless SOH channels are excluded by default");
    }

    std::map<GroupKey, ChannelGroup> grouped;
    for (auto &[key, acc] : channels) {
        (void)key;
        ChannelSeries series = std::move(acc.series);
        GroupKey gkey{series.network,
                      series.station,
                      series.location,
                      series.dimension,
                      series.sample_rate_hz,
                      options.gap_policy == GapPolicy::Ignore
                          ? format_btime(series.start_time)
                          : std::string{},
                      options.gap_policy == GapPolicy::Ignore
                          ? series.values.size()
                          : 0u};

        auto &group = grouped[gkey];
        if (group.channels.empty()) {
            group.network = series.network;
            group.station = series.station;
            group.location = series.location;
            group.sample_rate_hz = series.sample_rate_hz;
            group.dimension = series.dimension;
            group.start_time = series.start_time;
            group.start_time_us = series.start_time_us;
        }
        group.channels.push_back(std::move(series));
    }

    std::vector<ChannelGroup> result;
    result.reserve(grouped.size());
    for (auto &[key, group] : grouped) {
        (void)key;
        if (options.gap_policy != GapPolicy::Ignore)
            align_group_channels(group, options.gap_policy);
        std::sort(group.channels.begin(),
                  group.channels.end(),
                  [](const auto &a, const auto &b) {
                      return a.source_order < b.source_order;
                  });
        result.push_back(std::move(group));
    }
    return result;
}

void export_group_text(const ChannelGroup &group,
                       const std::string &output_path,
                       const TextExportOptions &options) {
    if (group.channels.empty()) {
        throw std::runtime_error("cannot export an empty channel group");
    }
    if (options.precision < 1 || options.precision > 17) {
        throw std::runtime_error("text precision must be between 1 and 17");
    }

    const std::size_t rows = group.sample_count();
    for (const auto &ch : group.channels) {
        if (ch.values.size() != rows) {
            throw std::runtime_error(
                "channels in export group have different sample counts");
        }
        if (!almost_equal(ch.sample_rate_hz, group.sample_rate_hz)) {
            throw std::runtime_error(
                "channels in export group have different sample rates");
        }
    }

    const auto order = ordered_channel_indices(group);
    std::ofstream out(output_path);
    if (!out) {
        throw std::runtime_error("cannot create output file: " + output_path);
    }
    out << std::setprecision(options.precision) << std::scientific;

    if (options.write_header) {
        for (std::size_t col = 0; col < order.size(); ++col) {
            if (col != 0)
                out << options.delimiter;
            out << group.channels[order[col]].channel;
        }
        out << '\n';
    }

    for (std::size_t row = 0; row < rows; ++row) {
        for (std::size_t col = 0; col < order.size(); ++col) {
            if (col != 0)
                out << options.delimiter;
            const double value = group.channels[order[col]].values[row];
            if (std::isnan(value)) {
                out << "NaN";
            } else {
                out << value;
            }
        }
        out << '\n';
    }
    if (!out) {
        throw std::runtime_error("failed while writing output file: "
                                 + output_path);
    }

    if (options.write_metadata) {
        const std::string meta_path = metadata_path_for(output_path);
        std::ofstream meta(meta_path);
        if (!meta) {
            throw std::runtime_error("cannot create metadata file: "
                                     + meta_path);
        }
        meta << "source_format=modified_miniseed\n";
        meta << "network=" << group.network << '\n';
        meta << "station=" << group.station << '\n';
        meta << "location=" << group.location << '\n';
        meta << "start_time_utc=" << format_btime(group.start_time) << '\n';
        meta << std::setprecision(15);
        meta << "sample_rate_hz=" << group.sample_rate_hz << '\n';
        meta << "sample_count=" << rows << '\n';
        meta << "dimension=" << dimension_name(group.dimension) << '\n';
        meta << "unit="
             << (group.channels.front().raw_counts
                     ? "count"
                     : physical_unit_name(group.dimension))
             << '\n';
        meta << "conversion="
             << (group.channels.front().raw_counts
                     ? "none (RawCounts)"
                     : "physical_value=count/sensitivity_si")
             << '\n';
        meta << "missing_value=NaN\n";
        meta << "column_count=" << order.size() << '\n';
        std::uint64_t total_missing = 0;
        for (const auto &ch : group.channels) {
            total_missing += ch.missing_sample_count;
        }
        meta << "missing_sample_count_across_columns=" << total_missing << '\n';
        for (std::size_t col = 0; col < order.size(); ++col) {
            const auto &ch = group.channels[order[col]];
            const auto column_number = col + 1;
            meta << "column_" << column_number << '=' << ch.network << '.'
                 << ch.station << '.' << ch.location << '.' << ch.channel
                 << ",sensitivity_raw=" << ch.sensitivity
                 << ",sensitivity_si=" << ch.actual_sensitivity
                 << ",records=" << ch.record_count
                 << ",valid_samples=" << ch.valid_sample_count
                 << ",missing_samples=" << ch.missing_sample_count
                 << ",internal_gap_count=" << ch.gaps.size()
                 << ",leading_missing=" << ch.leading_missing_sample_count
                 << ",trailing_missing=" << ch.trailing_missing_sample_count
                 << '\n';
            for (std::size_t gap_index = 0; gap_index < ch.gaps.size();
                 ++gap_index) {
                const auto &gap = ch.gaps[gap_index];
                meta << "column_" << column_number << "_gap_" << (gap_index + 1)
                     << "=before_record_start:"
                     << format_btime(gap.next_record_start)
                     << ",missing_samples:" << gap.missing_samples
                     << ",duration_seconds:" << gap.duration_seconds << '\n';
            }
        }
    }
}

std::string physical_unit_name(Dimension d) {
    switch (d) {
        case Dimension::Unknown:
            return "unknown";
        case Dimension::Dimensionless:
            return "1";
        case Dimension::Displacement:
            return "m";
        case Dimension::Velocity:
            return "m/s";
        case Dimension::Acceleration:
            return "m/s^2";
    }
    return "unknown";
}

} // namespace qrest_data::tools::mseed
