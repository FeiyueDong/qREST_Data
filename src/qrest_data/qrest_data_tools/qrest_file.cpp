#include "qrest_file.hpp"

#include <cmath>
#include <cstddef>
#include <cstring>
#include <fstream>
#include <iterator>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <string>

#include "data_packet.hpp"
#include "file_header.hpp"

namespace qrest_data::tools {
namespace {

std::string read_binary_file(const std::string &path) {
    std::ifstream input(path, std::ios::binary);
    if (!input.is_open()) {
        throw std::runtime_error("Cannot open input file: " + path);
    }
    return std::string((std::istreambuf_iterator<char>(input)),
                       std::istreambuf_iterator<char>());
}

void write_binary_file(const std::string &path, const std::string &bytes) {
    std::ofstream output(path, std::ios::binary);
    if (!output.is_open()) {
        throw std::runtime_error("Cannot create output file: " + path);
    }
    output.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
    if (!output) {
        throw std::runtime_error("Failed while writing output file: " + path);
    }
}

int parse_int(const std::string &text,
              std::size_t pos,
              std::size_t len,
              const char *field) {
    if (pos + len > text.size()) {
        throw std::runtime_error(std::string("Invalid timestamp: missing ")
                                 + field);
    }
    int value = 0;
    for (std::size_t i = pos; i < pos + len; ++i) {
        const char ch = text[i];
        if (ch < '0' || ch > '9') {
            throw std::runtime_error(std::string("Invalid timestamp: bad ")
                                     + field);
        }
        value = value * 10 + (ch - '0');
    }
    return value;
}

bool is_leap_year(int year) {
    return (year % 4 == 0 && year % 100 != 0) || (year % 400 == 0);
}

int days_in_month(int year, int month) {
    static constexpr int days[] = {
        31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    if (month < 1 || month > 12) {
        return 0;
    }
    if (month == 2 && is_leap_year(year)) {
        return 29;
    }
    return days[month - 1];
}

std::int64_t days_before_year(int year) {
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

std::int64_t days_before_month(int year, int month) {
    std::int64_t days = 0;
    for (int m = 1; m < month; ++m) {
        days += days_in_month(year, m);
    }
    return days;
}

void validate_metadata_dimensions(const Metadata &metadata) {
    if (metadata.InstrumentInfo.ChannelNum <= 0
        || metadata.DataInfo.NPTS <= 0) {
        throw std::runtime_error(
            "Metadata channel count and NPTS must be positive");
    }
    if (metadata.InstrumentInfo.Channels.size()
        != static_cast<std::size_t>(metadata.InstrumentInfo.ChannelNum)) {
        std::ostringstream oss;
        oss << "InstrumentInfo.ChannelNum ("
            << metadata.InstrumentInfo.ChannelNum
            << ") does not match Channels size ("
            << metadata.InstrumentInfo.Channels.size() << ")";
        throw std::runtime_error(oss.str());
    }
}

} // namespace

std::uint16_t sampling_rate_from_dt(double dt) {
    if (!(dt > 0.0) || !std::isfinite(dt)) {
        throw std::runtime_error("DataInfo.DT must be positive and finite");
    }

    const double rounded = std::round(1.0 / dt);
    if (!(rounded > 0.0)
        || rounded > static_cast<double>(
               std::numeric_limits<std::uint16_t>::max())) {
        throw std::runtime_error("Sampling rate is outside uint16 range");
    }
    return static_cast<std::uint16_t>(rounded);
}

std::uint64_t parse_iso8601_timestamp_ms(const std::string &value) {
    if (value.size() < 20 || value[4] != '-' || value[7] != '-'
        || value[10] != 'T' || value[13] != ':' || value[16] != ':') {
        throw std::runtime_error("StartTime must use ISO 8601 format");
    }

    const int year = parse_int(value, 0, 4, "year");
    const int month = parse_int(value, 5, 2, "month");
    const int day = parse_int(value, 8, 2, "day");
    const int hour = parse_int(value, 11, 2, "hour");
    const int minute = parse_int(value, 14, 2, "minute");
    const int second = parse_int(value, 17, 2, "second");

    if (month < 1 || month > 12 || day < 1 || day > days_in_month(year, month)
        || hour > 23 || minute > 59 || second > 60) {
        throw std::runtime_error("StartTime contains an out-of-range field");
    }

    std::size_t pos = 19;
    int millis = 0;
    if (pos < value.size() && value[pos] == '.') {
        ++pos;
        int digits = 0;
        while (pos < value.size() && value[pos] >= '0' && value[pos] <= '9') {
            if (digits < 3) {
                millis = millis * 10 + (value[pos] - '0');
            }
            ++digits;
            ++pos;
        }
        while (digits > 0 && digits < 3) {
            millis *= 10;
            ++digits;
        }
        if (digits == 0) {
            throw std::runtime_error("StartTime has an empty fractional part");
        }
    }

    int offset_minutes = 0;
    if (pos < value.size() && value[pos] == 'Z') {
        ++pos;
    } else if (pos + 6 <= value.size()
               && (value[pos] == '+' || value[pos] == '-')
               && value[pos + 3] == ':') {
        const int sign = value[pos] == '+' ? 1 : -1;
        const int offset_hour = parse_int(value, pos + 1, 2, "timezone hour");
        const int offset_minute =
            parse_int(value, pos + 4, 2, "timezone minute");
        if (offset_hour > 23 || offset_minute > 59) {
            throw std::runtime_error(
                "StartTime timezone offset is out of range");
        }
        offset_minutes = sign * (offset_hour * 60 + offset_minute);
        pos += 6;
    } else {
        throw std::runtime_error("StartTime must include Z or timezone offset");
    }

    if (pos != value.size()) {
        throw std::runtime_error("StartTime has unexpected trailing content");
    }

    std::int64_t days =
        days_before_year(year) + days_before_month(year, month) + (day - 1);
    std::int64_t seconds_since_epoch =
        days * 86400 + hour * 3600 + minute * 60 + second;
    seconds_since_epoch -= static_cast<std::int64_t>(offset_minutes) * 60;
    const std::int64_t total_ms = seconds_since_epoch * 1000 + millis;
    if (total_ms < 0) {
        throw std::runtime_error("StartTime must not be before Unix epoch");
    }
    return static_cast<std::uint64_t>(total_ms);
}

QrestFileData read_qrest_file(const std::string &path) {
    const std::string bytes = read_binary_file(path);
    if (bytes.size() < sizeof(FileHeaderPOD)) {
        throw std::runtime_error("Input is too short to contain qREST header");
    }

    const auto header =
        FileHeader::from_bytes(bytes.substr(0, sizeof(FileHeaderPOD)));
    const std::size_t expected_size = sizeof(FileHeaderPOD)
                                      + header.get_metadata_size()
                                      + header.get_data_size();
    if (bytes.size() != expected_size) {
        throw std::runtime_error(
            "qREST file length does not match file header");
    }

    const std::string metadata_bytes =
        bytes.substr(sizeof(FileHeaderPOD), header.get_metadata_size());
    const std::string packet_bytes =
        bytes.substr(sizeof(FileHeaderPOD) + header.get_metadata_size(),
                     header.get_data_size());

    QrestFileData result;
    result.file.metadata_size = header.get_metadata_size();
    result.file.data_size = header.get_data_size();
    result.file.file_size = bytes.size();
    result.metadata_json = metadata_bytes;
    result.metadata = Metadata::from_bytes(metadata_bytes);
    const DataPacket packet = DataPacket::from_bytes(packet_bytes);
    PacketHeaderPOD packet_header{};
    std::memcpy(&packet_header, packet_bytes.data(), sizeof(PacketHeaderPOD));
    result.packet.version = packet_header.version;
    result.packet.packet_type = packet_header.packet_type;
    result.packet.source_id = packet.get_source_id();
    result.packet.channel_count = packet.get_channel_count();
    result.packet.data_encoding = packet.get_data_encodings();
    result.packet.sampling_rate = packet.get_sampling_rate();
    result.packet.data_point_count = packet.get_data_point_count();
    result.packet.timestamp_ms = packet.get_timestamp();
    result.packet.body_size = packet_header.body_size;
    result.packet.checksum = packet_header.checksum;
    result.channel_sequential_data = packet.get_data();
    return result;
}

void write_qrest_file(const std::string &path,
                      const Metadata &metadata,
                      const std::vector<double> &channel_sequential_data,
                      std::uint16_t source_id,
                      std::uint16_t data_encoding) {
    write_qrest_file(path,
                     metadata.to_bytes(),
                     channel_sequential_data,
                     source_id,
                     data_encoding);
}

void write_qrest_file(const std::string &path,
                      const std::string &metadata_json,
                      const std::vector<double> &channel_sequential_data,
                      std::uint16_t source_id,
                      std::uint16_t data_encoding) {
    const Metadata metadata = Metadata::from_bytes(metadata_json);
    validate_metadata_dimensions(metadata);

    const auto channel_count =
        static_cast<std::uint16_t>(metadata.InstrumentInfo.ChannelNum);
    const auto npts = static_cast<std::uint32_t>(metadata.DataInfo.NPTS);
    const std::size_t expected_size =
        static_cast<std::size_t>(channel_count) * npts;
    if (channel_sequential_data.size() != expected_size) {
        std::ostringstream oss;
        oss << "Data size mismatch: expected " << expected_size
            << " values, got " << channel_sequential_data.size();
        throw std::runtime_error(oss.str());
    }

    const DataPacket packet(
        source_id,
        channel_count,
        data_encoding,
        sampling_rate_from_dt(metadata.DataInfo.DT),
        npts,
        parse_iso8601_timestamp_ms(metadata.DataInfo.StartTime),
        channel_sequential_data);
    const std::string packet_bytes = packet.to_bytes();
    const FileHeader header(static_cast<std::uint32_t>(metadata_json.size()),
                            static_cast<std::uint32_t>(packet_bytes.size()));

    write_binary_file(path, header.to_bytes() + metadata_json + packet_bytes);
}

} // namespace qrest_data::tools
