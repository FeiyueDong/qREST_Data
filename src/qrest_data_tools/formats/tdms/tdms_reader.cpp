#include "tdms_reader.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <ctime>
#include <iomanip>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <utility>

namespace qrest_data::tools::tdms {
namespace {

constexpr std::uint32_t kTocMetaData = 0x02;
constexpr std::uint32_t kTocNewObjList = 0x04;
constexpr std::uint32_t kTocRawData = 0x08;
constexpr std::uint32_t kTocInterleaved = 0x20;
constexpr std::uint32_t kTocBigEndian = 0x40;
constexpr std::uint32_t kTocDAQmxRawData = 0x80;
constexpr std::uint32_t kNoRawData = 0xFFFFFFFFu;
constexpr std::uint32_t kReuseRawDataIndex = 0u;
constexpr std::uint64_t kUnknownNextOffset =
    std::numeric_limits<std::uint64_t>::max();

std::uint16_t le_u16(const std::uint8_t *p) {
    return static_cast<std::uint16_t>(p[0])
           | (static_cast<std::uint16_t>(p[1]) << 8);
}

std::int16_t le_i16(const std::uint8_t *p) {
    return static_cast<std::int16_t>(le_u16(p));
}

std::uint32_t le_u32(const std::uint8_t *p) {
    return static_cast<std::uint32_t>(p[0])
           | (static_cast<std::uint32_t>(p[1]) << 8)
           | (static_cast<std::uint32_t>(p[2]) << 16)
           | (static_cast<std::uint32_t>(p[3]) << 24);
}

std::int32_t le_i32(const std::uint8_t *p) {
    return static_cast<std::int32_t>(le_u32(p));
}

std::uint64_t le_u64(const std::uint8_t *p) {
    return static_cast<std::uint64_t>(le_u32(p))
           | (static_cast<std::uint64_t>(le_u32(p + 4)) << 32);
}

std::int64_t le_i64(const std::uint8_t *p) {
    return static_cast<std::int64_t>(le_u64(p));
}

float le_f32(const std::uint8_t *p) {
    const auto bits = le_u32(p);
    float value{};
    std::memcpy(&value, &bits, sizeof(value));
    return value;
}

double le_f64(const std::uint8_t *p) {
    const auto bits = le_u64(p);
    double value{};
    std::memcpy(&value, &bits, sizeof(value));
    return value;
}

class BufferReader {
public:
    explicit BufferReader(const std::vector<std::uint8_t> &bytes)
        : bytes_(bytes) {}

    std::size_t remaining() const noexcept { return bytes_.size() - pos_; }

    std::uint8_t u8() {
        require(1);
        return bytes_[pos_++];
    }

    std::uint16_t u16() {
        require(2);
        const auto v = le_u16(bytes_.data() + pos_);
        pos_ += 2;
        return v;
    }

    std::uint32_t u32() {
        require(4);
        const auto v = le_u32(bytes_.data() + pos_);
        pos_ += 4;
        return v;
    }

    std::uint64_t u64() {
        require(8);
        const auto v = le_u64(bytes_.data() + pos_);
        pos_ += 8;
        return v;
    }

    std::int64_t i64() { return static_cast<std::int64_t>(u64()); }

    float f32() {
        require(4);
        const auto v = le_f32(bytes_.data() + pos_);
        pos_ += 4;
        return v;
    }

    double f64() {
        require(8);
        const auto v = le_f64(bytes_.data() + pos_);
        pos_ += 8;
        return v;
    }

    std::string string() {
        const auto length = u32();
        require(length);
        std::string s(reinterpret_cast<const char *>(bytes_.data() + pos_),
                      length);
        pos_ += length;
        return s;
    }

    void skip(std::size_t n) {
        require(n);
        pos_ += n;
    }

private:
    void require(std::size_t n) const {
        if (n > bytes_.size() - pos_) {
            throw std::runtime_error("truncated TDMS metadata");
        }
    }

    const std::vector<std::uint8_t> &bytes_;
    std::size_t pos_{};
};

PropertyValue read_property_value(BufferReader &r, DataType type) {
    PropertyValue result;
    result.data_type = type;

    switch (type) {
        case DataType::Int8:
            result.signed_value = static_cast<std::int8_t>(r.u8());
            break;
        case DataType::Int16:
            result.signed_value = static_cast<std::int16_t>(r.u16());
            break;
        case DataType::Int32:
            result.signed_value = static_cast<std::int32_t>(r.u32());
            break;
        case DataType::Int64:
            result.signed_value = r.i64();
            break;
        case DataType::UInt8:
            result.unsigned_value = r.u8();
            break;
        case DataType::UInt16:
            result.unsigned_value = r.u16();
            break;
        case DataType::UInt32:
            result.unsigned_value = r.u32();
            break;
        case DataType::UInt64:
            result.unsigned_value = r.u64();
            break;
        case DataType::Float32:
            result.float64_value = r.f32();
            break;
        case DataType::Float64:
            result.float64_value = r.f64();
            break;
        case DataType::String:
            result.string_value = r.string();
            break;
        case DataType::Boolean:
            result.unsigned_value = r.u8() ? 1u : 0u;
            break;
        case DataType::Timestamp:
            result.timestamp_value.fraction = r.u64();
            result.timestamp_value.seconds_since_1904 = r.i64();
            break;
        default:
            throw std::runtime_error("unsupported TDMS property type: "
                                     + data_type_name(type));
    }
    return result;
}

std::uint64_t checked_mul(std::uint64_t a, std::uint64_t b, const char *what) {
    if (a != 0 && b > std::numeric_limits<std::uint64_t>::max() / a) {
        throw std::runtime_error(
            std::string("TDMS size overflow while computing ") + what);
    }
    return a * b;
}

std::uint64_t checked_add(std::uint64_t a, std::uint64_t b, const char *what) {
    if (b > std::numeric_limits<std::uint64_t>::max() - a) {
        throw std::runtime_error(
            std::string("TDMS size overflow while computing ") + what);
    }
    return a + b;
}

std::size_t checked_size(std::uint64_t value, const char *what) {
    if (value
        > static_cast<std::uint64_t>(std::numeric_limits<std::size_t>::max())) {
        throw std::runtime_error(
            std::string("TDMS size does not fit size_t while reading ") + what);
    }
    return static_cast<std::size_t>(value);
}

std::uint64_t raw_index_byte_count(const RawDataIndex &index) {
    if (index.dimension != 1) {
        throw std::runtime_error(
            "only one-dimensional TDMS raw arrays are supported");
    }
    const auto size = fixed_type_size(index.data_type);
    if (size == 0) {
        throw std::runtime_error("variable/unsupported TDMS raw type: "
                                 + data_type_name(index.data_type));
    }
    return checked_mul(
        index.value_count, static_cast<std::uint64_t>(size), "raw-data bytes");
}

std::vector<std::uint8_t>
read_exact(std::ifstream &in, std::size_t n, const char *what) {
    std::vector<std::uint8_t> result(n);
    if (n == 0)
        return result;
    in.read(reinterpret_cast<char *>(result.data()),
            static_cast<std::streamsize>(n));
    if (in.gcount() != static_cast<std::streamsize>(n)) {
        throw std::runtime_error(std::string("truncated TDMS ") + what);
    }
    return result;
}

} // namespace

Reader::Reader(const std::string &path) : in_(path, std::ios::binary) {
    if (!in_) {
        throw std::runtime_error("cannot open TDMS file: " + path);
    }
    in_.seekg(0, std::ios::end);
    const auto end = in_.tellg();
    if (end < 0) {
        throw std::runtime_error("cannot determine TDMS file size");
    }
    file_size_ = static_cast<std::uint64_t>(end);
    in_.seekg(0, std::ios::beg);
}

bool Reader::next(Segment &segment) {
    if (next_file_offset_ == file_size_) {
        return false;
    }
    if (next_file_offset_ > file_size_) {
        throw std::runtime_error("TDMS segment offset exceeds file size");
    }
    if (file_size_ - next_file_offset_ < 28) {
        throw std::runtime_error("truncated TDMS lead-in");
    }

    in_.clear();
    in_.seekg(static_cast<std::streamoff>(next_file_offset_), std::ios::beg);
    const auto lead = read_exact(in_, 28, "lead-in");
    if (!(lead[0] == 'T' && lead[1] == 'D' && lead[2] == 'S'
          && lead[3] == 'm')) {
        throw std::runtime_error("invalid TDMS segment tag at file offset "
                                 + std::to_string(next_file_offset_));
    }

    segment = Segment{};
    segment.file_offset = next_file_offset_;
    segment.toc = le_u32(lead.data() + 4);
    segment.version = le_u32(lead.data() + 8);
    segment.next_segment_offset = le_u64(lead.data() + 12);
    segment.raw_data_offset = le_u64(lead.data() + 20);

    if (segment.version != 4712 && segment.version != 4713) {
        throw std::runtime_error("unsupported TDMS version: "
                                 + std::to_string(segment.version));
    }
    if ((segment.toc & kTocBigEndian) != 0) {
        throw std::runtime_error("big-endian TDMS segments are not supported");
    }
    if ((segment.toc & kTocDAQmxRawData) != 0) {
        throw std::runtime_error("DAQmx raw TDMS data is not supported");
    }
    if ((segment.toc & kTocInterleaved) != 0) {
        throw std::runtime_error(
            "interleaved TDMS raw data is not supported by this reader");
    }

    std::uint64_t segment_payload_size = segment.next_segment_offset;
    if (segment_payload_size == kUnknownNextOffset) {
        segment_payload_size = file_size_ - segment.file_offset - 28;
    }
    const std::uint64_t segment_end =
        checked_add(checked_add(segment.file_offset, 28, "segment end"),
                    segment_payload_size,
                    "segment end");
    if (segment_end > file_size_) {
        throw std::runtime_error("TDMS segment extends beyond file size");
    }
    if (segment.raw_data_offset > segment_payload_size) {
        throw std::runtime_error("TDMS raw-data offset exceeds segment size");
    }

    const bool has_metadata = (segment.toc & kTocMetaData) != 0;
    const bool has_raw_data = (segment.toc & kTocRawData) != 0;
    const bool new_object_list = (segment.toc & kTocNewObjList) != 0;

    if (has_metadata) {
        const auto metadata = read_exact(
            in_, checked_size(segment.raw_data_offset, "metadata"), "metadata");
        BufferReader r(metadata);
        const auto object_count = r.u32();
        segment.objects.reserve(object_count);
        std::vector<std::string> segment_raw_object_list;

        for (std::uint32_t i = 0; i < object_count; ++i) {
            ObjectMetadata object;
            object.path = r.string();
            const auto raw_index_length = r.u32();

            if (raw_index_length == kNoRawData) {
                object.has_raw_data = false;
            } else if (raw_index_length == kReuseRawDataIndex) {
                const auto it = raw_index_state_.find(object.path);
                if (it == raw_index_state_.end()) {
                    throw std::runtime_error(
                        "TDMS object reuses undefined raw-data index: "
                        + object.path);
                }
                object.has_raw_data = true;
                object.reused_raw_index = true;
                object.raw_index = it->second;
                segment_raw_object_list.push_back(object.path);
            } else {
                if (raw_index_length < 20) {
                    throw std::runtime_error(
                        "invalid TDMS raw-data index length for "
                        + object.path);
                }
                RawDataIndex index;
                index.data_type = static_cast<DataType>(r.u32());
                index.dimension = r.u32();
                index.value_count = r.u64();
                const auto consumed_including_length = 4u + 16u;
                if (raw_index_length > consumed_including_length) {
                    // String and some uncommon types append information to the
                    // raw index. They are not needed for the monitoring files;
                    // skip it so metadata still remains parseable, then reject
                    // only if raw decoding is requested.
                    r.skip(raw_index_length - consumed_including_length);
                }
                object.has_raw_data = true;
                object.raw_index = index;
                raw_index_state_[object.path] = index;
                segment_raw_object_list.push_back(object.path);
            }

            const auto property_count = r.u32();
            for (std::uint32_t p = 0; p < property_count; ++p) {
                const std::string name = r.string();
                const auto type = static_cast<DataType>(r.u32());
                object.properties.emplace(name, read_property_value(r, type));
            }
            segment.objects.push_back(std::move(object));
        }
        if (r.remaining() != 0) {
            throw std::runtime_error(
                "unexpected trailing bytes in TDMS metadata");
        }

        if (new_object_list) {
            active_raw_objects_ = std::move(segment_raw_object_list);
        }
    } else {
        // No metadata: raw object ordering and raw indexes carry over from the
        // previous segment according to TDMS semantics.
        if (segment.raw_data_offset != 0) {
            throw std::runtime_error(
                "TDMS segment without metadata has non-zero raw-data offset");
        }
    }

    const std::uint64_t raw_bytes_total =
        segment_payload_size - segment.raw_data_offset;
    if (has_raw_data) {
        if (active_raw_objects_.empty()) {
            throw std::runtime_error(
                "TDMS segment contains raw data but has no active raw objects");
        }

        std::uint64_t one_chunk_bytes = 0;
        for (const auto &path : active_raw_objects_) {
            const auto it = raw_index_state_.find(path);
            if (it == raw_index_state_.end()) {
                throw std::runtime_error("missing TDMS raw-data index for "
                                         + path);
            }
            one_chunk_bytes = checked_add(one_chunk_bytes,
                                          raw_index_byte_count(it->second),
                                          "raw-data chunk bytes");
        }
        if (one_chunk_bytes == 0 || raw_bytes_total % one_chunk_bytes != 0) {
            throw std::runtime_error(
                "TDMS raw-data size does not match active object indexes");
        }
        const std::uint64_t repetitions = raw_bytes_total / one_chunk_bytes;

        // Aggregate repeated chunks by object, preserving the final per-object
        // byte stream as if all values had been stored contiguously.
        std::map<std::string, std::size_t> output_index;
        for (const auto &path : active_raw_objects_) {
            RawObjectData raw;
            raw.path = path;
            raw.index = raw_index_state_.at(path);
            raw.index.value_count =
                checked_mul(raw.index.value_count,
                            repetitions,
                            "raw-data repeated value count");
            output_index[path] = segment.raw_objects.size();
            segment.raw_objects.push_back(std::move(raw));
        }

        for (std::uint64_t rep = 0; rep < repetitions; ++rep) {
            for (const auto &path : active_raw_objects_) {
                const auto &idx = raw_index_state_.at(path);
                const auto bytes_for_object = raw_index_byte_count(idx);
                auto part =
                    read_exact(in_,
                               checked_size(bytes_for_object, "raw data"),
                               "raw data");
                auto &dst = segment.raw_objects.at(output_index.at(path)).bytes;
                dst.insert(dst.end(), part.begin(), part.end());
            }
        }
    } else if (raw_bytes_total != 0) {
        throw std::runtime_error(
            "TDMS segment has raw bytes without raw-data ToC flag");
    }

    next_file_offset_ = segment_end;
    ++segment_index_;
    return true;
}

std::string data_type_name(DataType type) {
    switch (type) {
        case DataType::Int8:
            return "int8";
        case DataType::Int16:
            return "int16";
        case DataType::Int32:
            return "int32";
        case DataType::Int64:
            return "int64";
        case DataType::UInt8:
            return "uint8";
        case DataType::UInt16:
            return "uint16";
        case DataType::UInt32:
            return "uint32";
        case DataType::UInt64:
            return "uint64";
        case DataType::Float32:
            return "float32";
        case DataType::Float64:
            return "float64";
        case DataType::String:
            return "string";
        case DataType::Boolean:
            return "boolean";
        case DataType::Timestamp:
            return "timestamp";
        default: {
            std::ostringstream oss;
            oss << "unknown(0x" << std::hex << static_cast<std::uint32_t>(type)
                << ')';
            return oss.str();
        }
    }
}

std::size_t fixed_type_size(DataType type) {
    switch (type) {
        case DataType::Int8:
        case DataType::UInt8:
        case DataType::Boolean:
            return 1;
        case DataType::Int16:
        case DataType::UInt16:
            return 2;
        case DataType::Int32:
        case DataType::UInt32:
        case DataType::Float32:
            return 4;
        case DataType::Int64:
        case DataType::UInt64:
        case DataType::Float64:
            return 8;
        case DataType::Timestamp:
            return 16;
        case DataType::String:
            return 0;
        default:
            return 0;
    }
}

long double timestamp_unix_seconds(const Timestamp &timestamp) {
    // 1904-01-01 to 1970-01-01 = 2082844800 seconds.
    constexpr std::int64_t tdms_to_unix = 2082844800LL;
    constexpr long double two64 = 18446744073709551616.0L;
    return static_cast<long double>(timestamp.seconds_since_1904 - tdms_to_unix)
           + static_cast<long double>(timestamp.fraction) / two64;
}

std::string format_timestamp_utc(const Timestamp &timestamp,
                                 int fractional_digits) {
    if (fractional_digits < 0 || fractional_digits > 9) {
        throw std::runtime_error("timestamp fractional digits must be 0..9");
    }

    const long double unix_seconds_ld = timestamp_unix_seconds(timestamp);
    const auto whole_seconds =
        static_cast<std::int64_t>(std::floor(unix_seconds_ld));
    const long double frac =
        unix_seconds_ld - static_cast<long double>(whole_seconds);

    std::time_t t = static_cast<std::time_t>(whole_seconds);
    std::tm tm{};
#if defined(_WIN32)
    gmtime_s(&tm, &t);
#else
    gmtime_r(&t, &tm);
#endif

    std::ostringstream oss;
    oss << std::put_time(&tm, "%Y-%m-%dT%H:%M:%S");
    if (fractional_digits > 0) {
        std::uint64_t scale = 1;
        for (int i = 0; i < fractional_digits; ++i)
            scale *= 10;
        auto units = static_cast<std::uint64_t>(
            std::llround(frac * static_cast<long double>(scale)));
        if (units >= scale)
            units = scale - 1;
        oss << '.' << std::setw(fractional_digits) << std::setfill('0')
            << units;
    }
    oss << 'Z';
    return oss.str();
}

std::vector<std::int32_t> decode_int32(const RawObjectData &raw) {
    if (raw.index.data_type != DataType::Int32) {
        throw std::runtime_error("TDMS object " + raw.path + " is not int32");
    }
    if (raw.bytes.size() % 4 != 0) {
        throw std::runtime_error("misaligned int32 TDMS raw data for "
                                 + raw.path);
    }
    std::vector<std::int32_t> values(raw.bytes.size() / 4);
    for (std::size_t i = 0; i < values.size(); ++i) {
        values[i] = static_cast<std::int32_t>(le_u32(raw.bytes.data() + i * 4));
    }
    return values;
}

std::vector<double> decode_float64(const RawObjectData &raw) {
    if (raw.index.data_type != DataType::Float64) {
        throw std::runtime_error("TDMS object " + raw.path + " is not float64");
    }
    if (raw.bytes.size() % 8 != 0) {
        throw std::runtime_error("misaligned float64 TDMS raw data for "
                                 + raw.path);
    }
    std::vector<double> values(raw.bytes.size() / 8);
    for (std::size_t i = 0; i < values.size(); ++i) {
        values[i] = le_f64(raw.bytes.data() + i * 8);
    }
    return values;
}

std::vector<double> decode_numeric(const RawObjectData &raw) {
    const auto type_size = fixed_type_size(raw.index.data_type);
    if (type_size == 0 || raw.index.data_type == DataType::Timestamp
        || raw.index.data_type == DataType::String
        || raw.index.data_type == DataType::Boolean) {
        throw std::runtime_error("TDMS object " + raw.path
                                 + " is not a numeric scalar type");
    }
    if (raw.bytes.size() % type_size != 0) {
        throw std::runtime_error("misaligned numeric TDMS raw data for "
                                 + raw.path);
    }

    std::vector<double> values(raw.bytes.size() / type_size);
    for (std::size_t i = 0; i < values.size(); ++i) {
        const auto *p = raw.bytes.data() + i * type_size;
        switch (raw.index.data_type) {
            case DataType::Int8:
                values[i] = static_cast<double>(static_cast<std::int8_t>(*p));
                break;
            case DataType::Int16:
                values[i] = static_cast<double>(le_i16(p));
                break;
            case DataType::Int32:
                values[i] = static_cast<double>(le_i32(p));
                break;
            case DataType::Int64:
                values[i] = static_cast<double>(le_i64(p));
                break;
            case DataType::UInt8:
                values[i] = static_cast<double>(*p);
                break;
            case DataType::UInt16:
                values[i] = static_cast<double>(le_u16(p));
                break;
            case DataType::UInt32:
                values[i] = static_cast<double>(le_u32(p));
                break;
            case DataType::UInt64:
                values[i] = static_cast<double>(le_u64(p));
                break;
            case DataType::Float32:
                values[i] = static_cast<double>(le_f32(p));
                break;
            case DataType::Float64:
                values[i] = le_f64(p);
                break;
            case DataType::String:
            case DataType::Boolean:
            case DataType::Timestamp:
                throw std::runtime_error("TDMS object " + raw.path
                                         + " is not a numeric scalar type");
        }
    }
    return values;
}

std::vector<Timestamp> decode_timestamp(const RawObjectData &raw) {
    if (raw.index.data_type != DataType::Timestamp) {
        throw std::runtime_error("TDMS object " + raw.path
                                 + " is not timestamp");
    }
    if (raw.bytes.size() % 16 != 0) {
        throw std::runtime_error("misaligned timestamp TDMS raw data for "
                                 + raw.path);
    }
    std::vector<Timestamp> values(raw.bytes.size() / 16);
    for (std::size_t i = 0; i < values.size(); ++i) {
        const auto *p = raw.bytes.data() + i * 16;
        values[i].fraction = le_u64(p);
        values[i].seconds_since_1904 = le_i64(p + 8);
    }
    return values;
}

} // namespace qrest_data::tools::tdms
