#ifndef QREST_DATA_TOOLS_QREST_FILE_HPP
#define QREST_DATA_TOOLS_QREST_FILE_HPP

#include <cstdint>
#include <string>
#include <vector>

#include "metadata.hpp"

namespace qrest_data::tools {

struct FileInfo {
    std::uint32_t metadata_size{};
    std::uint32_t data_size{};
    std::uint64_t file_size{};
};

struct PacketInfo {
    std::uint8_t version{};
    std::uint8_t packet_type{};
    std::uint16_t source_id{};
    std::uint16_t channel_count{};
    std::uint16_t data_encoding{};
    std::uint16_t sampling_rate{};
    std::uint32_t data_point_count{};
    std::uint64_t timestamp_ms{};
    std::uint32_t body_size{};
    std::uint32_t checksum{};
};

struct QrestFileData {
    FileInfo file;
    Metadata metadata;
    std::string metadata_json;
    PacketInfo packet;
    std::vector<double> channel_sequential_data;
};

QrestFileData read_qrest_file(const std::string &path);

void write_qrest_file(const std::string &path,
                      const Metadata &metadata,
                      const std::vector<double> &channel_sequential_data,
                      std::uint16_t source_id,
                      std::uint16_t data_encoding);

void write_qrest_file(const std::string &path,
                      const std::string &metadata_json,
                      const std::vector<double> &channel_sequential_data,
                      std::uint16_t source_id,
                      std::uint16_t data_encoding);

std::uint16_t sampling_rate_from_dt(double dt);
std::uint64_t parse_iso8601_timestamp_ms(const std::string &value);

} // namespace qrest_data::tools

#endif // QREST_DATA_TOOLS_QREST_FILE_HPP
