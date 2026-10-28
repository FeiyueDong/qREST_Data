/**
**            qREST - Quick Response Evaluation for Safety Tagging
**     Institute of Engineering Mechanics, China Earthquake Administration
**                 Copyright 2024 - 2026 QLab, Dong Feiyue
**                          All Rights Reserved.
** Project: qREST
** File: /src/qrest_data_tools/formats/mseed/modified_mseed.cpp
** File Created: not recorded in existing source
** Author: not recorded in existing source
** Last Modified: Wednesday, 28 October 2026
** Modified By: Codex
*/
#include "modified_mseed.hpp"
#include "../../../common/utf8_path.hpp"
#include "steim2.hpp"

#include <algorithm>
#include <bit>
#include <chrono>
#include <cmath>
#include <iomanip>
#include <limits>
#include <set>
#include <sstream>
#include <stdexcept>

namespace qrest_data::tools::mseed {
namespace {
std::string fixed_string(const std::uint8_t *p, std::size_t n) {
    std::string s(reinterpret_cast<const char *>(p), n);
    while (!s.empty() && (s.back() == '\0' || s.back() == ' '))
        s.pop_back();
    return s;
}
std::uint16_t u16(const std::uint8_t *p, bool le) {
    return le ? (unsigned(p[1]) << 8) | p[0] : (unsigned(p[0]) << 8) | p[1];
}
std::uint32_t u32(const std::uint8_t *p, bool le) {
    std::uint32_t v = 0;
    for (int i = 0; i < 4; ++i)
        v = (v << 8) | p[le ? 3 - i : i];
    return v;
}
bool plausible_date(std::span<const std::uint8_t> b, bool le) {
    const auto y = u16(b.data() + 20, le), d = u16(b.data() + 22, le);
    return y >= 1900 && y <= 2200 && d >= 1 && d <= 366;
}
Header fixed_header(std::span<const std::uint8_t> b) {
    if (b.size() >= 3 && b[0] == 'M' && b[1] == 'S' && b[2] == 3)
        throw std::runtime_error("MiniSEED 3 is not supported");
    if (b.size() < 48)
        throw std::runtime_error("truncated MiniSEED fixed header");
    Header h;
    h.waveform_id = fixed_string(b.data(), 2);
    const bool modified =
        h.waveform_id == "wc" || h.waveform_id == "wt" || h.waveform_id == "ws";
    if (!modified) {
        const bool sequence = std::all_of(b.begin(), b.begin() + 6, [](auto c) {
            return (c >= '0' && c <= '9') || c == ' ';
        });
        if (!sequence
            || std::string("DRQM").find(char(b[6])) == std::string::npos
            || b[7] != ' ')
            throw std::runtime_error("unrecognized MiniSEED 2 fixed header");
        h.standard_v2 = true;
        h.header_little_endian =
            !plausible_date(b, false) && plausible_date(b, true);
    } else {
        h.packet_word = u32(b.data() + 2, false);
        h.packet_length_index = (h.packet_word >> 29) & 7;
        h.sequence = h.packet_word & 0x1fffffff;
    }
    const bool le = h.header_little_endian;
    h.quality_indicator = char(b[6]);
    h.reserved_char = char(b[7]);
    h.station = fixed_string(b.data() + 8, 5);
    h.location = fixed_string(b.data() + 13, 2);
    h.channel = fixed_string(b.data() + 15, 3);
    h.network = fixed_string(b.data() + 18, 2);
    h.start_time = {u16(b.data() + 20, le),
                    u16(b.data() + 22, le),
                    b[24],
                    b[25],
                    b[26],
                    u16(b.data() + 28, le)};
    h.start_time_us = btime_unix_us(h.start_time);
    h.sample_count = u16(b.data() + 30, le);
    h.sample_rate_factor = static_cast<std::int16_t>(u16(b.data() + 32, le));
    h.sample_rate_multiplier =
        static_cast<std::int16_t>(u16(b.data() + 34, le));
    h.activity_flags = b[36];
    h.io_clock_flags = b[37];
    h.data_quality_flags = b[38];
    h.blockette_count = b[39];
    h.time_correction = static_cast<std::int32_t>(u32(b.data() + 40, le));
    h.data_offset = u16(b.data() + 44, le);
    h.first_blockette_offset = u16(b.data() + 46, le);
    if (h.data_offset < 48 || h.first_blockette_offset < 48
        || h.first_blockette_offset >= h.data_offset)
        throw std::runtime_error(
            "invalid MiniSEED data offset or Blockette offset");
    if ((h.activity_flags & 2) == 0)
        h.start_time_us += std::int64_t(h.time_correction) * 100;
    return h;
}
Header parse_header(std::span<const std::uint8_t> b) {
    Header h = fixed_header(b);
    const bool le = h.header_little_endian;
    std::set<std::size_t> seen;
    std::size_t pos = h.first_blockette_offset, count = 0, extension = 0;
    while (pos) {
        if (!seen.insert(pos).second || pos < 48 || pos + 4 > h.data_offset
            || pos + 4 > b.size())
            throw std::runtime_error("invalid/cyclic MiniSEED Blockette chain");
        const auto type = u16(b.data() + pos, le),
                   next = u16(b.data() + pos + 2, le);
        std::size_t length =
            type == 100 ? 12 : (type == 1000 || type == 1001 ? 8 : 4);
        if (pos + length > b.size() || pos + length > h.data_offset
            || (next && next < pos + length))
            throw std::runtime_error(
                "truncated or overlapping MiniSEED Blockette");
        if (type == 1000) {
            if (h.record_length)
                throw std::runtime_error("duplicate Blockette 1000");
            h.blockette_type = type;
            h.next_blockette_offset = next;
            h.encoding = b[pos + 4];
            h.word_order = b[pos + 5];
            h.record_length_exponent = b[pos + 6];
            h.blockette_reserved = b[pos + 7];
            if (h.record_length_exponent < 7 || h.record_length_exponent > 20)
                throw std::runtime_error(
                    "unsupported MiniSEED record length exponent");
            h.record_length = std::size_t{1} << h.record_length_exponent;
            extension = pos + 8;
        } else if (type == 1001) {
            if (h.has_blockette_1001)
                throw std::runtime_error("duplicate Blockette 1001");
            h.has_blockette_1001 = true;
            h.start_time_us += static_cast<std::int8_t>(b[pos + 5]);
        } else if (type == 100) {
            if (h.actual_sample_rate_hz > 0)
                throw std::runtime_error("duplicate Blockette 100");
            h.actual_sample_rate_hz =
                std::bit_cast<float>(u32(b.data() + pos + 4, le));
            if (!(h.actual_sample_rate_hz > 0)
                || !std::isfinite(h.actual_sample_rate_hz))
                throw std::runtime_error("invalid Blockette 100 sample rate");
        }
        ++count;
        pos = next;
    }
    if (count != h.blockette_count || !h.record_length)
        throw std::runtime_error(
            "MiniSEED Blockette count mismatch or missing Blockette 1000");
    if (h.data_offset > h.record_length
        || (h.encoding == 11 && h.data_offset % 64) || h.word_order > 1)
        throw std::runtime_error(
            "invalid MiniSEED data offset/frame alignment/word order");
    if (!h.standard_v2) {
        if (h.first_blockette_offset != 48 || h.blockette_count != 1
            || extension + 8 > h.data_offset || extension + 8 > b.size())
            throw std::runtime_error(
                "unsupported Modified MiniSEED extension layout");
        if ((std::size_t{1} << (h.packet_length_index + 5)) != h.record_length)
            throw std::runtime_error(
                "packet-length index disagrees with Blockette 1000");
        h.has_extension = true;
        h.channel_order = fixed_string(b.data() + extension, 3);
        h.extension_byte = b[extension + 3];
        h.dimension_sensitivity_raw = u32(b.data() + extension + 4, false);
        h.dimension = static_cast<Dimension>(h.dimension_sensitivity_raw >> 30);
        h.sensitivity = h.dimension_sensitivity_raw & 0x3fffffff;
    }
    h.start_time = btime_from_unix_us(h.start_time_us);
    return h;
}
} // namespace

std::int64_t btime_unix_us(const BTime &t) {
    using namespace std::chrono;
    const year y{int(t.year)};
    if (t.year < 1900 || t.year > 2200 || !t.day_of_year
        || t.day_of_year > (y.is_leap() ? 366 : 365) || t.hour > 23
        || t.minute > 59 || t.second > 59 || t.fraction_0001s > 9999)
        throw std::runtime_error(
            "invalid/unsupported MiniSEED BTime (including leap seconds)");
    return duration_cast<microseconds>(
               (sys_days{y / January / 1} + days{t.day_of_year - 1}
                + hours{t.hour} + minutes{t.minute} + seconds{t.second})
                   .time_since_epoch())
               .count()
           + std::int64_t(t.fraction_0001s) * 100;
}
BTime btime_from_unix_us(std::int64_t t) {
    using namespace std::chrono;
    const sys_time<microseconds> tp{microseconds{t}};
    const auto day = floor<days>(tp);
    const year_month_day date{day};
    const hh_mm_ss clock{tp - day};
    return {
        std::uint16_t(int(date.year())),
        std::uint16_t((day - sys_days{date.year() / January / 1}).count() + 1),
        std::uint8_t(clock.hours().count()),
        std::uint8_t(clock.minutes().count()),
        std::uint8_t(clock.seconds().count()),
        std::uint16_t(clock.subseconds().count() / 100)};
}
Header parse_record_header(std::span<const std::uint8_t> bytes) {
    return parse_header(bytes);
}
Record parse_record(std::span<const std::uint8_t> bytes) {
    Header h = parse_header(bytes);
    if (bytes.size() != h.record_length)
        throw std::runtime_error(
            "MiniSEED record length mismatch/truncated record");
    if (h.encoding != 11)
        throw std::runtime_error("unsupported MiniSEED encoding: "
                                 + std::to_string(h.encoding));
    return {h, decode_steim2(bytes, h)};
}
Reader::Reader(const std::string &path)
    : in_(qrest_data::detail::path_from_utf8(path), std::ios::binary) {
    if (!in_)
        throw std::runtime_error("cannot open file: " + path);
}
bool Reader::next(Record &record) {
    const auto offset = in_.tellg();
    std::vector<std::uint8_t> bytes(48);
    in_.read(reinterpret_cast<char *>(bytes.data()), 48);
    if (!in_.gcount() && in_.eof())
        return false;
    try {
        if (in_.gcount() != 48) {
            bytes.resize(static_cast<std::size_t>(in_.gcount()));
            fixed_header(bytes);
            throw std::runtime_error("truncated record header");
        }
        const Header fixed = fixed_header(bytes);
        const auto extend = [&](std::size_t n) {
            const auto old = bytes.size();
            if (n < old)
                throw std::runtime_error("invalid record boundary");
            bytes.resize(n);
            in_.read(reinterpret_cast<char *>(bytes.data() + old),
                     std::streamsize(n - old));
            if (in_.gcount() != std::streamsize(n - old))
                throw std::runtime_error("truncated record body");
        };
        extend(fixed.data_offset);
        const Header h = parse_header(bytes);
        extend(h.record_length);
        record = parse_record(bytes);
    } catch (const std::exception &e) {
        throw std::runtime_error(
            "MiniSEED record " + std::to_string(record_index_) + " at byte "
            + std::to_string(static_cast<std::streamoff>(offset)) + ": "
            + e.what());
    }
    ++record_index_;
    return true;
}

double sample_rate_hz(std::int16_t factor, std::int16_t multiplier) {
    if (factor == 0 || multiplier == 0) {
        return 0.0;
    }

    double rate = factor > 0 ? static_cast<double>(factor)
                             : 1.0 / static_cast<double>(-factor);
    if (multiplier > 0) {
        rate *= static_cast<double>(multiplier);
    } else {
        rate /= static_cast<double>(-multiplier);
    }
    return rate;
}

double sample_rate_hz(const Header &header) {
    if (header.actual_sample_rate_hz > 0)
        return header.actual_sample_rate_hz;
    return sample_rate_hz(header.sample_rate_factor,
                          header.sample_rate_multiplier);
}

std::string format_btime(const BTime &t) {
    std::ostringstream oss;
    oss << std::setfill('0') << std::setw(4) << t.year << "-DOY" << std::setw(3)
        << t.day_of_year << 'T' << std::setw(2) << static_cast<unsigned>(t.hour)
        << ':' << std::setw(2) << static_cast<unsigned>(t.minute) << ':'
        << std::setw(2) << static_cast<unsigned>(t.second) << '.'
        << std::setw(4) << t.fraction_0001s << 'Z';
    return oss.str();
}

std::string dimension_name(Dimension d) {
    switch (d) {
        case Dimension::Unknown:
            return "unknown";
        case Dimension::Dimensionless:
            return "dimensionless";
        case Dimension::Displacement:
            return "displacement";
        case Dimension::Velocity:
            return "velocity";
        case Dimension::Acceleration:
            return "acceleration";
    }
    return "unknown";
}

double count_to_physical(std::int32_t count, std::uint32_t sensitivity_raw) {
    if (sensitivity_raw == 0) {
        throw std::runtime_error("cannot convert count with zero sensitivity");
    }

    // The protocol stores the sensitivity as:
    //   actual sensitivity [count / SI-unit] * 100
    // Therefore the physical value is:
    //   count / (sensitivity_raw / 100)
    // = count * 100 / sensitivity_raw.
    return static_cast<double>(count) * 100.0
           / static_cast<double>(sensitivity_raw);
}

} // namespace qrest_data::tools::mseed
