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

constexpr long double kBTimeToleranceSeconds = 0.0001001L;

bool is_leap_year(int year) {
    return (year % 4 == 0 && year % 100 != 0) || (year % 400 == 0);
}

std::int64_t days_before_year(int year) {
    // Sufficient for MiniSEED years in normal seismic monitoring data.
    std::int64_t days = 0;
    if (year >= 1970) {
        for (int y = 1970; y < year; ++y) {
            days += is_leap_year(y) ? 366 : 365;
        }
    } else {
        for (int y = 1969; y >= year; --y) {
            days -= is_leap_year(y) ? 366 : 365;
        }
    }
    return days;
}

long double btime_seconds(const BTime &t) {
    if (t.year == 0 || t.day_of_year == 0 || t.day_of_year > 366 || t.hour > 23
        || t.minute > 59 || t.second > 60) {
        throw std::runtime_error("invalid BTime while assembling channel data");
    }

    const std::int64_t days = days_before_year(static_cast<int>(t.year))
                              + static_cast<std::int64_t>(t.day_of_year - 1u);
    long double seconds = static_cast<long double>(days) * 86400.0L;
    seconds += static_cast<long double>(t.hour) * 3600.0L;
    seconds += static_cast<long double>(t.minute) * 60.0L;
    seconds += static_cast<long double>(t.second);
    seconds += static_cast<long double>(t.fraction_0001s) * 0.0001L;
    return seconds;
}

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

std::int64_t time_offset_to_samples(long double seconds,
                                    double sample_rate,
                                    const std::string &context) {
    if (!(sample_rate > 0.0) || !std::isfinite(sample_rate)) {
        throw std::runtime_error("invalid sample rate while checking "
                                 + context);
    }

    const long double exact_samples =
        seconds * static_cast<long double>(sample_rate);
    if (exact_samples
            > static_cast<long double>(std::numeric_limits<std::int64_t>::max())
        || exact_samples < static_cast<long double>(
               std::numeric_limits<std::int64_t>::min())) {
        throw std::runtime_error("time discontinuity is too large while "
                                 "checking "
                                 + context);
    }

    const auto rounded = static_cast<std::int64_t>(std::llround(exact_samples));
    const long double snapped_seconds = static_cast<long double>(rounded)
                                        / static_cast<long double>(sample_rate);
    const long double residual = std::fabs(seconds - snapped_seconds);

    if (residual > kBTimeToleranceSeconds) {
        std::ostringstream oss;
        oss << "time discontinuity in " << context
            << " is not aligned to the sampling grid: delta="
            << std::setprecision(12) << static_cast<double>(seconds)
            << " s, nearest sample offset=" << rounded
            << ", residual=" << static_cast<double>(residual) << " s";
        throw std::runtime_error(oss.str());
    }
    return rounded;
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
    long double expected_next_seconds{};
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

void align_group_channels(ChannelGroup &group) {
    if (group.channels.empty()) {
        return;
    }

    long double group_start_seconds =
        std::numeric_limits<long double>::infinity();
    long double group_end_seconds =
        -std::numeric_limits<long double>::infinity();
    BTime earliest_btime{};
    bool have_earliest = false;

    for (const auto &ch : group.channels) {
        const long double start = btime_seconds(ch.start_time);
        const long double end =
            start
            + static_cast<long double>(ch.values.size())
                  / static_cast<long double>(ch.sample_rate_hz);
        if (!have_earliest || start < group_start_seconds) {
            group_start_seconds = start;
            earliest_btime = ch.start_time;
            have_earliest = true;
        }
        group_end_seconds = std::max(group_end_seconds, end);
    }

    group.start_time = earliest_btime;

    for (auto &ch : group.channels) {
        const long double original_start = btime_seconds(ch.start_time);
        const long double original_end =
            original_start
            + static_cast<long double>(ch.values.size())
                  / static_cast<long double>(ch.sample_rate_hz);

        const long double lead_seconds = original_start - group_start_seconds;
        const std::int64_t lead_samples = time_offset_to_samples(
            lead_seconds,
            ch.sample_rate_hz,
            "leading alignment of stream " + stream_name(ch));
        if (lead_samples < 0) {
            throw std::runtime_error(
                "internal error while aligning leading samples of "
                + stream_name(ch));
        }
        prepend_missing(ch, static_cast<std::uint64_t>(lead_samples));

        const long double trail_seconds = group_end_seconds - original_end;
        const std::int64_t trail_samples = time_offset_to_samples(
            trail_seconds,
            ch.sample_rate_hz,
            "trailing alignment of stream " + stream_name(ch));
        if (trail_samples < 0) {
            throw std::runtime_error(
                "internal error while aligning trailing samples of "
                + stream_name(ch));
        }
        append_missing(ch, static_cast<std::uint64_t>(trail_samples));
        ch.trailing_missing_sample_count +=
            static_cast<std::uint64_t>(trail_samples);
        ch.start_time = group.start_time;
    }

    const std::size_t expected_rows = group.channels.front().values.size();
    for (const auto &ch : group.channels) {
        if (ch.values.size() != expected_rows) {
            throw std::runtime_error("failed to align channel lengths in group "
                                     + group.network + "." + group.station + "."
                                     + group.location);
        }
    }
}

} // namespace

std::size_t ChannelGroup::sample_count() const noexcept {
    return channels.empty() ? 0u : channels.front().values.size();
}

std::vector<std::size_t> ordered_channel_indices(const ChannelGroup &group) {
    std::vector<std::size_t> result;
    result.reserve(group.channels.size());

    std::string order;
    for (const auto &ch : group.channels) {
        if (!ch.channel_order.empty()) {
            order = ch.channel_order;
            break;
        }
    }

    std::vector<bool> used(group.channels.size(), false);
    for (char desired : order) {
        for (std::size_t i = 0; i < group.channels.size(); ++i) {
            if (used[i] || group.channels[i].channel.empty()) {
                continue;
            }
            if (group.channels[i].channel.back() == desired) {
                result.push_back(i);
                used[i] = true;
                break;
            }
        }
    }

    std::vector<std::size_t> rest;
    for (std::size_t i = 0; i < group.channels.size(); ++i) {
        if (!used[i]) {
            rest.push_back(i);
        }
    }
    std::sort(rest.begin(), rest.end(), [&](std::size_t a, std::size_t b) {
        return group.channels[a].channel < group.channels[b].channel;
    });
    result.insert(result.end(), rest.begin(), rest.end());
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
    std::map<ChannelKey, Accumulator> channels;

    while (reader.next(record)) {
        const Header &h = record.header;

        if (h.dimension == Dimension::Dimensionless
            && !options.include_dimensionless) {
            continue;
        }
        if (h.sensitivity == 0) {
            throw std::runtime_error("zero sensitivity for selected stream "
                                     + stream_name(h));
        }

        const double fs = sample_rate_hz(h);
        if (!(fs > 0.0) || !std::isfinite(fs)) {
            throw std::runtime_error("invalid sample rate for selected stream "
                                     + stream_name(h));
        }

        ChannelKey key{h.network, h.station, h.location, h.channel};
        auto &acc = channels[key];
        const long double record_start = btime_seconds(h.start_time);
        long double effective_record_start = record_start;

        if (!acc.initialized) {
            acc.initialized = true;
            acc.series.network = h.network;
            acc.series.station = h.station;
            acc.series.location = h.location;
            acc.series.channel = h.channel;
            acc.series.channel_order = h.channel_order;
            acc.series.start_time = h.start_time;
            acc.series.last_record_time = h.start_time;
            acc.series.sample_rate_hz = fs;
            acc.series.dimension = h.dimension;
            acc.series.sensitivity = h.sensitivity;
        } else {
            if (!almost_equal(acc.series.sample_rate_hz, fs)) {
                throw std::runtime_error("sample rate changed within stream "
                                         + stream_name(h));
            }
            if (acc.series.dimension != h.dimension) {
                throw std::runtime_error("dimension changed within stream "
                                         + stream_name(h));
            }
            if (acc.series.sensitivity != h.sensitivity) {
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
                const long double delta =
                    record_start - acc.expected_next_seconds;

                if (std::fabs(delta) <= kBTimeToleranceSeconds) {
                    effective_record_start = acc.expected_next_seconds;
                } else {
                    const std::string context = "stream " + stream_name(h);
                    const std::int64_t offset_samples =
                        time_offset_to_samples(delta, fs, context);

                    if (offset_samples < 0) {
                        std::ostringstream oss;
                        oss << "time overlap in " << context
                            << ": record starts at "
                            << format_btime(h.start_time)
                            << ", overlap=" << (-offset_samples) << " samples ("
                            << std::setprecision(12)
                            << static_cast<double>(-delta) << " s)";
                        throw std::runtime_error(oss.str());
                    }

                    if (offset_samples > 0) {
                        if (options.gap_policy == GapPolicy::Error) {
                            std::ostringstream oss;
                            oss << "time discontinuity in " << context
                                << ": record starts at "
                                << format_btime(h.start_time)
                                << ", missing=" << offset_samples
                                << " samples (" << std::setprecision(12)
                                << (static_cast<double>(offset_samples) / fs)
                                << " s)";
                            throw std::runtime_error(oss.str());
                        }

                        append_missing(
                            acc.series,
                            static_cast<std::uint64_t>(offset_samples));
                        acc.series.gaps.push_back(
                            DataGap{h.start_time,
                                    static_cast<std::uint64_t>(offset_samples),
                                    static_cast<double>(
                                        static_cast<long double>(offset_samples)
                                        / static_cast<long double>(fs))});

                        effective_record_start =
                            acc.expected_next_seconds
                            + static_cast<long double>(offset_samples)
                                  / static_cast<long double>(fs);
                    }
                }
            }
        }

        acc.series.values.reserve(acc.series.values.size()
                                  + record.samples.size());
        for (std::int32_t count : record.samples) {
            acc.series.values.push_back(
                count_to_physical(count, h.sensitivity));
        }
        acc.series.valid_sample_count += record.samples.size();
        ++acc.series.record_count;
        acc.series.last_record_time = h.start_time;
        if (options.gap_policy == GapPolicy::Ignore) {
            acc.expected_next_seconds =
                record_start
                + static_cast<long double>(record.samples.size())
                      / static_cast<long double>(fs);
        } else {
            acc.expected_next_seconds =
                effective_record_start
                + static_cast<long double>(record.samples.size())
                      / static_cast<long double>(fs);
        }
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
        }
        group.channels.push_back(std::move(series));
    }

    std::vector<ChannelGroup> result;
    result.reserve(grouped.size());
    for (auto &[key, group] : grouped) {
        (void)key;
        if (options.gap_policy != GapPolicy::Ignore) {
            align_group_channels(group);
        }
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
        meta << "unit=" << physical_unit_name(group.dimension) << '\n';
        meta << "conversion=physical_value=count*100/sensitivity_raw\n";
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
                 << ",sensitivity_raw=" << ch.sensitivity << ",sensitivity_si="
                 << (static_cast<double>(ch.sensitivity) / 100.0)
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
