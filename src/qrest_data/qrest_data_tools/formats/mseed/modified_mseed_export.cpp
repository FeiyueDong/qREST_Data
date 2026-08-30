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
    std::string start_time;
    std::size_t sample_count{};

    bool operator<(const GroupKey &rhs) const {
        return std::tie(network,
                        station,
                        location,
                        dimension,
                        sample_rate,
                        start_time,
                        sample_count)
               < std::tie(rhs.network,
                          rhs.station,
                          rhs.location,
                          rhs.dimension,
                          rhs.sample_rate,
                          rhs.start_time,
                          rhs.sample_count);
    }
};

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
            acc.expected_next_seconds = record_start;
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
        }

        if (options.verify_time_continuity) {
            // BTime resolution is 0.1 ms. Allow one BTime tick plus a tiny
            // floating-point margin when comparing adjacent record starts.
            const long double tolerance = 0.0001001L;
            if (std::fabs(record_start - acc.expected_next_seconds)
                > tolerance) {
                std::ostringstream oss;
                oss << "time discontinuity in stream " << stream_name(h)
                    << ": record starts at " << format_btime(h.start_time)
                    << " but previous samples imply a different start time";
                throw std::runtime_error(oss.str());
            }
        }

        acc.series.values.reserve(acc.series.values.size()
                                  + record.samples.size());
        for (std::int32_t count : record.samples) {
            acc.series.values.push_back(
                count_to_physical(count, h.sensitivity));
        }
        ++acc.series.record_count;
        acc.series.last_record_time = h.start_time;
        acc.expected_next_seconds =
            record_start
            + static_cast<long double>(record.samples.size())
                  / static_cast<long double>(fs);
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
                      format_btime(series.start_time),
                      series.values.size()};

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
            out << group.channels[order[col]].values[row];
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
        meta << "column_count=" << order.size() << '\n';
        for (std::size_t col = 0; col < order.size(); ++col) {
            const auto &ch = group.channels[order[col]];
            meta << "column_" << (col + 1) << '=' << ch.network << '.'
                 << ch.station << '.' << ch.location << '.' << ch.channel
                 << ",sensitivity_raw=" << ch.sensitivity << ",sensitivity_si="
                 << (static_cast<double>(ch.sensitivity) / 100.0)
                 << ",records=" << ch.record_count << '\n';
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
