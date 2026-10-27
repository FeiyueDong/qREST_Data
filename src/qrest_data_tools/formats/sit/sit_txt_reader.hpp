#pragma once
#include <array>
#include <cstdint>
#include <string>
#include <vector>
namespace qrest_data::tools::sit {
struct TextData {
    std::array<std::string, 3> labels;
    std::array<std::vector<double>, 3> values;
    std::int64_t start_time_us{};
    double sample_rate_hz{};
    std::string encoding;
};
// Fixed SIT Gal profile; timezone is an explicit numeric UTC offset.
TextData read_txt(const std::string &path, const std::string &timezone);
} // namespace qrest_data::tools::sit
