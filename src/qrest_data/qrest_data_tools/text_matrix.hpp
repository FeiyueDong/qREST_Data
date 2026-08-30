#ifndef QREST_DATA_TOOLS_TEXT_MATRIX_HPP
#define QREST_DATA_TOOLS_TEXT_MATRIX_HPP

#include <cstddef>
#include <string>
#include <vector>

namespace qrest_data::tools {

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
