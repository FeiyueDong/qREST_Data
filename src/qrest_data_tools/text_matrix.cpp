#include "text_matrix.hpp"

#include <fstream>
#include <iomanip>
#include <sstream>
#include <stdexcept>

namespace qrest_data::tools {

std::vector<double> read_time_major_text_matrix(const std::string &path,
                                                std::size_t channel_count,
                                                std::size_t sample_count) {
    if (channel_count == 0 || sample_count == 0) {
        throw std::runtime_error("Text matrix dimensions must be positive");
    }

    std::ifstream input(path);
    if (!input.is_open()) {
        throw std::runtime_error("Cannot open data file: " + path);
    }

    std::vector<double> result(channel_count * sample_count);
    std::string line;
    std::size_t row = 0;
    while (std::getline(input, line)) {
        if (line.empty()) {
            continue;
        }
        if (row >= sample_count) {
            throw std::runtime_error(
                "Text matrix has more rows than DataInfo.NPTS");
        }

        std::stringstream row_stream(line);
        for (std::size_t col = 0; col < channel_count; ++col) {
            double value = 0.0;
            if (!(row_stream >> value)) {
                std::ostringstream oss;
                oss << "Text matrix row " << (row + 1)
                    << " has fewer columns than InstrumentInfo.ChannelNum";
                throw std::runtime_error(oss.str());
            }
            result[col * sample_count + row] = value;
        }

        double extra = 0.0;
        if (row_stream >> extra) {
            std::ostringstream oss;
            oss << "Text matrix row " << (row + 1)
                << " has more columns than InstrumentInfo.ChannelNum";
            throw std::runtime_error(oss.str());
        }

        ++row;
    }

    if (row != sample_count) {
        std::ostringstream oss;
        oss << "Text matrix row count mismatch: expected " << sample_count
            << ", got " << row;
        throw std::runtime_error(oss.str());
    }

    return result;
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

    std::ofstream output(path);
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
