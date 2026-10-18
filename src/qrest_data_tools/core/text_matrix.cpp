/**
**            qREST - Quick Response Evaluation for Safety Tagging
**     Institute of Engineering Mechanics, China Earthquake Administration
**                 Copyright 2024 - 2026 QLab, Dong Feiyue
**                          All Rights Reserved.
** Project: qREST
** File: /src/qrest_data_tools/core/text_matrix.cpp
** File Created: not recorded in existing source
** Author: not recorded in existing source
** Last Modified: Sunday, 18 October 2026
** Modified By: Codex
*/
#include "text_matrix.hpp"

#include <cmath>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <limits>
#include <sstream>
#include <stdexcept>

namespace qrest_data::tools {

std::vector<double> read_text_matrix(const std::string &path,
                                     std::size_t channels,
                                     std::size_t samples,
                                     bool channel_major,
                                     char delimiter) {
    if (!channels || !samples
        || channels > std::numeric_limits<std::size_t>::max() / samples)
        throw std::runtime_error("Invalid source text dimensions");
    std::ifstream input(
        std::filesystem::path(std::u8string(path.begin(), path.end())));
    if (!input)
        throw std::runtime_error("Cannot open data file: " + path);
    std::vector<double> out(channels * samples);
    const auto rows = channel_major ? channels : samples,
               cols = channel_major ? samples : channels;
    std::string line;
    std::size_t row = 0;
    while (std::getline(input, line)) {
        if (line.find_first_not_of(" \t\r") == std::string::npos)
            continue;
        if (row >= rows)
            throw std::runtime_error("Source text has extra rows");
        std::istringstream stream(line);
        for (std::size_t col = 0; col < cols; ++col) {
            double value{};
            if (delimiter) {
                std::string cell;
                if (!std::getline(stream, cell, delimiter))
                    throw std::runtime_error("Missing source text cell");
                std::istringstream token(cell);
                if (!(token >> value) || !(token >> std::ws).eof())
                    throw std::runtime_error("Invalid source text number");
            } else if (!(stream >> value))
                throw std::runtime_error("Missing/invalid source text number");
            if (!std::isfinite(value))
                throw std::runtime_error("Non-finite source text number");
            out[channel_major ? row * samples + col : col * samples + row] =
                value;
        }
        if (!(stream >> std::ws).eof()
            || (delimiter && !line.empty() && line.back() == delimiter))
            throw std::runtime_error("Source text has extra columns");
        ++row;
    }
    if (!input.eof() || row != rows)
        throw std::runtime_error("Source text row count mismatch");
    return out;
}
std::vector<double> read_time_major_text_matrix(const std::string &path,
                                                std::size_t channels,
                                                std::size_t samples) {
    return read_text_matrix(path, channels, samples, false);
}

void write_time_major_text_matrix(const std::string &path,
                                  const std::vector<double> &channel_data,
                                  std::size_t channel_count,
                                  std::size_t sample_count,
                                  int precision) {
    if (channel_count == 0 || sample_count == 0) {
        throw std::runtime_error("Text matrix dimensions must be positive");
    }
    if (channel_data.size() != channel_count * sample_count) {
        throw std::runtime_error(
            "Channel-sequential data size does not match dimensions");
    }
    if (precision < 1 || precision > 17) {
        throw std::runtime_error("Text precision must be between 1 and 17");
    }

    std::ofstream output(
        std::filesystem::path(std::u8string(path.begin(), path.end())));
    if (!output.is_open()) {
        throw std::runtime_error("Cannot create data output file: " + path);
    }
    output << std::fixed << std::setprecision(precision);

    for (std::size_t row = 0; row < sample_count; ++row) {
        for (std::size_t col = 0; col < channel_count; ++col) {
            if (col != 0) {
                output << ' ';
            }
            output << channel_data[col * sample_count + row];
        }
        output << '\n';
    }

    if (!output) {
        throw std::runtime_error("Failed while writing data output file: "
                                 + path);
    }
}

} // namespace qrest_data::tools
