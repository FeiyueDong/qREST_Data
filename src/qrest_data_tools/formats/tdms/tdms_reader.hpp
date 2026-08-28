#pragma once

#include <cstdint>
#include <fstream>
#include <map>
#include <optional>
#include <string>
#include <vector>

namespace qrest_data::tools::tdms {

// TDMS primitive types used by the supplied monitoring files.
enum class DataType : std::uint32_t {
    Int8 = 0x01,
    Int16 = 0x02,
    Int32 = 0x03,
    Int64 = 0x04,
    UInt8 = 0x05,
    UInt16 = 0x06,
    UInt32 = 0x07,
    UInt64 = 0x08,
    Float32 = 0x09,
    Float64 = 0x0A,
    String = 0x20,
    Boolean = 0x21,
    Timestamp = 0x44,
};

struct Timestamp {
    // TDMS timestamp epoch is 1904-01-01 00:00:00 UTC.
    std::int64_t seconds_since_1904{};
    std::uint64_t fraction{}; // fraction / 2^64 seconds
};

struct RawDataIndex {
    DataType data_type{DataType::Int32};
    std::uint32_t dimension{1};
    std::uint64_t value_count{};
};

struct PropertyValue {
    DataType data_type{DataType::String};
    std::string string_value;
    double float64_value{};
    std::int64_t signed_value{};
    std::uint64_t unsigned_value{};
    Timestamp timestamp_value{};
};

struct ObjectMetadata {
    std::string path;
    bool has_raw_data{false};
    bool reused_raw_index{false};
    std::optional<RawDataIndex> raw_index;
    std::map<std::string, PropertyValue> properties;
};

struct RawObjectData {
    std::string path;
    RawDataIndex index;
    std::vector<std::uint8_t> bytes;
};

struct Segment {
    std::uint64_t file_offset{};
    std::uint32_t toc{};
    std::uint32_t version{};
    std::uint64_t next_segment_offset{};
    std::uint64_t raw_data_offset{};
    std::vector<ObjectMetadata> objects;
    std::vector<RawObjectData> raw_objects;
};

class Reader {
public:
    explicit Reader(const std::string &path);

    // Returns false only at clean EOF. Throws on malformed/unsupported TDMS.
    bool next(Segment &segment);

    std::uint64_t file_size() const noexcept { return file_size_; }
    std::size_t segment_index() const noexcept { return segment_index_; }

private:
    std::ifstream in_;
    std::uint64_t file_size_{};
    std::uint64_t next_file_offset_{};
    std::size_t segment_index_{};

    std::map<std::string, RawDataIndex> raw_index_state_;
    std::vector<std::string> active_raw_objects_;
};

std::string data_type_name(DataType type);
std::size_t fixed_type_size(DataType type);
std::string format_timestamp_utc(const Timestamp &timestamp,
                                 int fractional_digits = 6);
long double timestamp_unix_seconds(const Timestamp &timestamp);

std::vector<std::int32_t> decode_int32(const RawObjectData &raw);
std::vector<double> decode_float64(const RawObjectData &raw);
std::vector<double> decode_numeric(const RawObjectData &raw);
std::vector<Timestamp> decode_timestamp(const RawObjectData &raw);

} // namespace qrest_data::tools::tdms
