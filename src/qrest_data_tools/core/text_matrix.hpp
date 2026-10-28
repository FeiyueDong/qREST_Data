/**
**            qREST - Quick Response Evaluation for Safety Tagging
**     Institute of Engineering Mechanics, China Earthquake Administration
**                 Copyright 2024 - 2026 QLab, Dong Feiyue
**                          All Rights Reserved.
** Project: qREST
** File: /src/qrest_data_tools/core/text_matrix.hpp
** File Created: not recorded in existing source
** Author: not recorded in existing source
** Last Modified: Wednesday, 28 October 2026
** Modified By: Codex
*/
#ifndef QREST_DATA_TOOLS_TEXT_MATRIX_HPP
#define QREST_DATA_TOOLS_TEXT_MATRIX_HPP

#include <cstddef>
#include <string>
#include <vector>

namespace qrest_data::tools {

struct TextMatrix {
    std::size_t channels{}, samples{};
    std::vector<double> values;
};
// Newlines define rows. Space/Tab/comma/semicolon delimit numbers; dimensions
// are inferred from a rectangular matrix. Values are channel sequential.
TextMatrix read_channel_text_matrix(const std::string &path, bool transpose);

// Explicit source layout; delimiter=0 means whitespace. Always returns
// channel-sequential samples.
std::vector<double> read_text_matrix(const std::string &path,
                                     std::size_t channels,
                                     std::size_t samples,
                                     bool channel_major,
                                     char delimiter = 0);

std::vector<double> read_time_major_text_matrix(const std::string &path,
                                                std::size_t channel_count,
                                                std::size_t sample_count);

void write_time_major_text_matrix(const std::string &path,
                                  const std::vector<double> &channel_data,
                                  std::size_t channel_count,
                                  std::size_t sample_count,
                                  int precision = 8);

} // namespace qrest_data::tools

#endif // QREST_DATA_TOOLS_TEXT_MATRIX_HPP
