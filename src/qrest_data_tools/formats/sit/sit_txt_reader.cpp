#include "sit_txt_reader.hpp"
#include <algorithm>
#include <charconv>
#include <chrono>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <set>
#include <stdexcept>
#include <string_view>

namespace qrest_data::tools::sit {
namespace {
std::array<std::string_view, 4> split(const std::string &line) {
    std::array<std::string_view, 4> fields;
    std::string_view rest = line;
    for (std::size_t i = 0; i < 4; ++i) {
        const auto p = rest.find('\t');
        if ((i < 3 && p == rest.npos) || (i == 3 && p != rest.npos))
            throw std::runtime_error(
                "SIT TXT requires exactly four Tab-separated columns");
        fields[i] = rest.substr(0, p);
        if (i < 3)
            rest.remove_prefix(p + 1);
    }
    return fields;
}
int number(std::string_view s) {
    int n{};
    const auto r = std::from_chars(s.data(), s.data() + s.size(), n);
    if (s.empty() || r.ec != std::errc{} || r.ptr != s.data() + s.size()
        || !std::all_of(
            s.begin(), s.end(), [](char c) { return c >= '0' && c <= '9'; }))
        throw std::runtime_error("invalid timestamp digits");
    return n;
}
std::int64_t timestamp(std::string_view s) {
    using namespace std::chrono;
    if (s.size() != 23 || s[4] != '/' || s[7] != '/' || s[10] != ' '
        || s[13] != ':' || s[16] != ':' || s[19] != '.')
        throw std::runtime_error(
            "invalid SIT timestamp; expected YYYY/MM/DD HH:MM:SS.mmm");
    const year_month_day d{year{number(s.substr(0, 4))},
                           month{unsigned(number(s.substr(5, 2)))},
                           day{unsigned(number(s.substr(8, 2)))}};
    const auto h = number(s.substr(11, 2)), m = number(s.substr(14, 2)),
               sec = number(s.substr(17, 2));
    if (!d.ok() || h > 23 || m > 59 || sec > 59)
        throw std::runtime_error("invalid SIT calendar/time");
    return duration_cast<microseconds>((sys_days{d} + hours{h} + minutes{m}
                                        + seconds{sec}
                                        + milliseconds{number(s.substr(20, 3))})
                                           .time_since_epoch())
        .count();
}
int timezone_minutes(std::string_view s) {
    if (s.size() != 6 || (s[0] != '+' && s[0] != '-') || s[3] != ':')
        throw std::runtime_error(
            "timezone must be a numeric offset such as +08:00");
    const int h = number(s.substr(1, 2)), m = number(s.substr(4, 2));
    if (h > 14 || m > 59 || (h == 14 && m))
        throw std::runtime_error("invalid timezone offset");
    return (s[0] == '-' ? -1 : 1) * (h * 60 + m);
}
} // namespace
TextData read_txt(const std::string &path, const std::string &timezone) {
    const auto offset = std::int64_t(timezone_minutes(timezone)) * 60000000;
    std::ifstream in(path, std::ios::binary);
    std::string line;
    if (!std::getline(in, line))
        throw std::runtime_error("SIT TXT line 1: missing header");
    if (!line.empty() && line.back() == '\r')
        line.pop_back();
    if (line.starts_with("\xef\xbb\xbf"))
        line.erase(0, 3);
    TextData result;
    try {
        const auto fields = split(line);
        const bool gbk = fields[3] == "\xca\xb1\xbc\xe4";
        if (!gbk && fields[3] != "时间")
            throw std::runtime_error("unsupported SIT time header");
        result.encoding = gbk ? "GBK" : "UTF-8";
        const std::string suffix = gbk ? "\xcf\xf2" : "向";
        const std::array<std::string, 3> axes{"UD", "NS", "EW"};
        std::set<std::string> used;
        for (std::size_t i = 0; i < 3; ++i) {
            for (const auto &axis : axes)
                if (fields[i] == axis + suffix)
                    result.labels[i] = axis;
            if (result.labels[i].empty()
                || !used.insert(result.labels[i]).second)
                throw std::runtime_error(
                    "unsupported or duplicate SIT direction header");
        }
    } catch (const std::exception &e) {
        throw std::runtime_error("SIT TXT line 1: " + std::string(e.what()));
    }
    std::int64_t previous{}, dt{};
    std::size_t rows = 0;
    for (std::size_t line_no = 2; std::getline(in, line); ++line_no) {
        if (!line.empty() && line.back() == '\r')
            line.pop_back();
        // Empty lines are rejected so corruption cannot remove samples
        // silently.
        try {
            const auto fields = split(line);
            for (std::size_t i = 0; i < 3; ++i) {
                double v{};
                auto field = fields[i];
                if (field.starts_with('+'))
                    field.remove_prefix(1);
                const auto r = std::from_chars(
                    field.data(), field.data() + field.size(), v);
                if (field.empty() || r.ec != std::errc{}
                    || r.ptr != field.data() + field.size()
                    || !std::isfinite(v))
                    throw std::runtime_error(
                        "invalid/non-finite numeric value in column "
                        + std::to_string(i + 1));
                result.values[i].push_back(v);
            }
            const auto t = timestamp(fields[3]) - offset;
            if (!rows)
                result.start_time_us = t;
            else if (rows == 1) {
                dt = t - previous;
                if (dt <= 0)
                    throw std::runtime_error("duplicate/reversed timestamp");
            } else if (t - previous != dt)
                throw std::runtime_error(
                    "timestamp gap/overlap or sample interval changed");
            previous = t;
            ++rows;
        } catch (const std::exception &e) {
            throw std::runtime_error("SIT TXT line " + std::to_string(line_no)
                                     + ": " + e.what());
        }
    }
    if (!in.eof())
        throw std::runtime_error("failed reading SIT TXT");
    if (rows < 2)
        throw std::runtime_error(
            "SIT TXT requires at least two samples to establish the time axis");
    result.sample_rate_hz = 1000000.0 / double(dt);
    // A declared export rate is cross-checked; file names never define the time
    // axis.
    const auto filename = std::filesystem::path(path).filename().string();
    const auto p = filename.rfind("_SRate_");
    if (p != std::string::npos) {
        const auto end = filename.find('_', p + 7);
        const std::string_view rate(
            filename.data() + p + 7,
            (end == std::string::npos ? filename.size() : end) - (p + 7));
        const auto nominal = number(rate);
        if (std::abs(result.sample_rate_hz - nominal) > 1e-9)
            throw std::runtime_error("SIT TXT line 3 timestamp interval "
                                     "conflicts with SRate export marker");
    }
    return result;
}
} // namespace qrest_data::tools::sit
