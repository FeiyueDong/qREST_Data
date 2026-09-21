#include "modified_mseed.hpp"

#include <algorithm>
#include <array>
#include <iomanip>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <utility>

namespace qrest_data::tools::mseed {
namespace {

std::uint16_t be_u16(const std::uint8_t *p) {
    return static_cast<std::uint16_t>((static_cast<std::uint16_t>(p[0]) << 8)
                                      | static_cast<std::uint16_t>(p[1]));
}

std::int16_t be_i16(const std::uint8_t *p) {
    return static_cast<std::int16_t>(be_u16(p));
}

std::uint32_t be_u32(const std::uint8_t *p) {
    return (static_cast<std::uint32_t>(p[0]) << 24)
           | (static_cast<std::uint32_t>(p[1]) << 16)
           | (static_cast<std::uint32_t>(p[2]) << 8)
           | static_cast<std::uint32_t>(p[3]);
}

std::int32_t be_i32(const std::uint8_t *p) {
    return static_cast<std::int32_t>(be_u32(p));
}

std::uint32_t le_u32(const std::uint8_t *p) {
    return (static_cast<std::uint32_t>(p[3]) << 24)
           | (static_cast<std::uint32_t>(p[2]) << 16)
           | (static_cast<std::uint32_t>(p[1]) << 8)
           | static_cast<std::uint32_t>(p[0]);
}

std::string fixed_string(const std::uint8_t *p, std::size_t n) {
    std::string s(reinterpret_cast<const char *>(p), n);
    while (!s.empty() && (s.back() == '\0' || s.back() == ' ')) {
        s.pop_back();
    }
    return s;
}

std::streamsize checked_streamsize(std::size_t value, const char *what) {
    if (value > static_cast<std::size_t>(
            std::numeric_limits<std::streamsize>::max())) {
        throw std::runtime_error(std::string("MiniSEED read size too large: ")
                                 + what);
    }
    return static_cast<std::streamsize>(value);
}

std::int32_t sign_extend(std::uint32_t value, unsigned bits) {
    if (bits == 0 || bits > 32) {
        throw std::runtime_error("invalid sign-extension width");
    }
    if (bits == 32) {
        return static_cast<std::int32_t>(value);
    }

    const std::uint32_t mask = (std::uint32_t{1} << bits) - 1u;
    value &= mask;
    const std::uint32_t sign = std::uint32_t{1} << (bits - 1u);
    if ((value & sign) != 0u) {
        value |= ~mask;
    }
    return static_cast<std::int32_t>(value);
}

void append_steim2_word(std::uint32_t word,
                        std::uint8_t control_code,
                        std::vector<std::int32_t> &diffs) {
    switch (control_code) {
        case 0:
            return;

        case 1: // 4 x 8-bit differences
            diffs.push_back(sign_extend(word >> 24, 8));
            diffs.push_back(sign_extend(word >> 16, 8));
            diffs.push_back(sign_extend(word >> 8, 8));
            diffs.push_back(sign_extend(word, 8));
            return;

        case 2: {
            const std::uint8_t dnib =
                static_cast<std::uint8_t>((word >> 30) & 0x3u);
            switch (dnib) {
                case 1: // 1 x 30-bit
                    diffs.push_back(sign_extend(word, 30));
                    return;
                case 2: // 2 x 15-bit
                    diffs.push_back(sign_extend(word >> 15, 15));
                    diffs.push_back(sign_extend(word, 15));
                    return;
                case 3: // 3 x 10-bit
                    diffs.push_back(sign_extend(word >> 20, 10));
                    diffs.push_back(sign_extend(word >> 10, 10));
                    diffs.push_back(sign_extend(word, 10));
                    return;
                default:
                    throw std::runtime_error(
                        "invalid Steim-2 dnib for control code 2");
            }
        }

        case 3: {
            const std::uint8_t dnib =
                static_cast<std::uint8_t>((word >> 30) & 0x3u);
            switch (dnib) {
                case 0: // 5 x 6-bit
                    for (int shift : {24, 18, 12, 6, 0}) {
                        diffs.push_back(sign_extend(word >> shift, 6));
                    }
                    return;
                case 1: // 6 x 5-bit
                    for (int shift : {25, 20, 15, 10, 5, 0}) {
                        diffs.push_back(sign_extend(word >> shift, 5));
                    }
                    return;
                case 2: // 7 x 4-bit; bits 29..28 are unused
                    for (int shift : {24, 20, 16, 12, 8, 4, 0}) {
                        diffs.push_back(sign_extend(word >> shift, 4));
                    }
                    return;
                default:
                    throw std::runtime_error(
                        "invalid Steim-2 dnib for control code 3");
            }
        }

        default:
            throw std::runtime_error("invalid Steim-2 control code");
    }
}

std::vector<std::int32_t> decode_steim2(const std::vector<std::uint8_t> &record,
                                        const Header &h) {
    if (h.encoding != 11) {
        throw std::runtime_error(
            "unsupported encoding: only Steim-2 (11) is implemented");
    }
    if (h.data_offset >= record.size()) {
        throw std::runtime_error("data offset is outside the record");
    }

    const std::size_t compressed_bytes = record.size() - h.data_offset;
    if ((compressed_bytes % 64u) != 0u) {
        throw std::runtime_error(
            "Steim data region is not an integer number of 64-byte frames");
    }

    auto read_word = [&](const std::uint8_t *p) -> std::uint32_t {
        if (h.word_order == 1) {
            return be_u32(p);
        }
        if (h.word_order == 0) {
            return le_u32(p);
        }
        throw std::runtime_error("invalid Blockette 1000 word-order flag");
    };

    std::vector<std::int32_t> diffs;
    diffs.reserve(static_cast<std::size_t>(h.sample_count) + 32u);

    std::int32_t x0 = 0;
    std::int32_t xn = 0;
    bool got_constants = false;

    const std::size_t frame_count = compressed_bytes / 64u;
    for (std::size_t frame = 0; frame < frame_count; ++frame) {
        const std::uint8_t *frame_ptr =
            record.data() + h.data_offset + frame * 64u;
        std::array<std::uint32_t, 16> words{};
        for (std::size_t i = 0; i < 16; ++i) {
            words[i] = read_word(frame_ptr + i * 4u);
        }

        const std::uint32_t control = words[0];
        std::size_t first_data_word = 1;
        if (frame == 0) {
            x0 = static_cast<std::int32_t>(words[1]);
            xn = static_cast<std::int32_t>(words[2]);
            got_constants = true;
            first_data_word = 3;
        }

        for (std::size_t word_index = first_data_word; word_index < 16;
             ++word_index) {
            const unsigned shift = static_cast<unsigned>(30u - 2u * word_index);
            const auto code =
                static_cast<std::uint8_t>((control >> shift) & 0x3u);
            append_steim2_word(words[word_index], code, diffs);
        }
    }

    if (!got_constants) {
        throw std::runtime_error("Steim-2 record has no frame 0");
    }

    std::vector<std::int32_t> samples;
    samples.reserve(h.sample_count);
    if (h.sample_count == 0) {
        return samples;
    }

    // In Steim, X0 is stored explicitly. The first packed difference is d0,
    // which connects the previous record to X0, so decompression of this
    // standalone record starts at diffs[1].
    if (h.sample_count > 1 && diffs.size() < h.sample_count) {
        throw std::runtime_error(
            "not enough Steim-2 differences for declared sample count");
    }

    samples.push_back(x0);
    for (std::size_t i = 1; i < h.sample_count; ++i) {
        const std::int64_t next = static_cast<std::int64_t>(samples.back())
                                  + static_cast<std::int64_t>(diffs[i]);
        if (next < std::numeric_limits<std::int32_t>::min()
            || next > std::numeric_limits<std::int32_t>::max()) {
            throw std::runtime_error("Steim-2 integration overflow");
        }
        samples.push_back(static_cast<std::int32_t>(next));
    }

    if (!samples.empty() && samples.back() != xn) {
        std::ostringstream oss;
        oss << "Steim-2 reverse integration constant mismatch: decoded "
            << samples.back() << ", header Xn " << xn;
        throw std::runtime_error(oss.str());
    }

    return samples;
}

Header parse_header(const std::vector<std::uint8_t> &bytes) {
    if (bytes.size() < 64) {
        throw std::runtime_error(
            "record is shorter than the 64-byte protocol header");
    }

    Header h;
    h.waveform_id = fixed_string(bytes.data() + 0, 2);
    if (h.waveform_id != "wc" && h.waveform_id != "wt"
        && h.waveform_id != "ws") {
        throw std::runtime_error("unknown waveform identifier: '"
                                 + h.waveform_id + "'");
    }

    h.packet_word = be_u32(bytes.data() + 2);
    h.packet_length_index =
        static_cast<std::uint8_t>((h.packet_word >> 29) & 0x7u);
    h.sequence = h.packet_word & 0x1FFFFFFFu;

    h.quality_indicator = static_cast<char>(bytes[6]);
    h.reserved_char = static_cast<char>(bytes[7]);
    h.station = fixed_string(bytes.data() + 8, 5);
    h.location = fixed_string(bytes.data() + 13, 2);
    h.channel = fixed_string(bytes.data() + 15, 3);
    h.network = fixed_string(bytes.data() + 18, 2);

    h.start_time.year = be_u16(bytes.data() + 20);
    h.start_time.day_of_year = be_u16(bytes.data() + 22);
    h.start_time.hour = bytes[24];
    h.start_time.minute = bytes[25];
    h.start_time.second = bytes[26];
    h.start_time.fraction_0001s = be_u16(bytes.data() + 28);

    h.sample_count = be_u16(bytes.data() + 30);
    h.sample_rate_factor = be_i16(bytes.data() + 32);
    h.sample_rate_multiplier = be_i16(bytes.data() + 34);
    h.activity_flags = bytes[36];
    h.io_clock_flags = bytes[37];
    h.data_quality_flags = bytes[38];
    h.blockette_count = bytes[39];
    h.time_correction = be_i32(bytes.data() + 40);
    h.data_offset = be_u16(bytes.data() + 44);
    h.first_blockette_offset = be_u16(bytes.data() + 46);

    // This reader intentionally targets the supplied protocol: Blockette 1000
    // immediately follows the 48-byte modified fixed header.
    if (h.first_blockette_offset != 48) {
        throw std::runtime_error(
            "unsupported first Blockette offset (expected 48)");
    }

    const std::size_t b = h.first_blockette_offset;
    h.blockette_type = be_u16(bytes.data() + b + 0);
    h.next_blockette_offset = be_u16(bytes.data() + b + 2);
    h.encoding = bytes[b + 4];
    h.word_order = bytes[b + 5];
    h.record_length_exponent = bytes[b + 6];
    h.blockette_reserved = bytes[b + 7];

    if (h.blockette_type != 1000) {
        throw std::runtime_error("first Blockette is not Blockette 1000");
    }
    if (h.record_length_exponent >= sizeof(std::size_t) * 8u) {
        throw std::runtime_error("record length exponent is too large");
    }
    h.record_length = std::size_t{1} << h.record_length_exponent;
    if (h.record_length < 64) {
        throw std::runtime_error(
            "record length is smaller than protocol header");
    }

    const std::size_t ext = b + 8;
    h.channel_order = fixed_string(bytes.data() + ext, 3);
    h.extension_byte = bytes[ext + 3];
    h.dimension_sensitivity_raw = be_u32(bytes.data() + ext + 4);
    h.dimension =
        static_cast<Dimension>((h.dimension_sensitivity_raw >> 30) & 0x3u);
    h.sensitivity = h.dimension_sensitivity_raw & 0x3FFFFFFFu;

    if (h.data_offset < 64 || h.data_offset > h.record_length) {
        throw std::runtime_error("invalid data offset");
    }

    return h;
}

} // namespace

Reader::Reader(const std::string &path) : in_(path, std::ios::binary) {
    if (!in_) {
        throw std::runtime_error("cannot open file: " + path);
    }
}

bool Reader::next(Record &record) {
    std::array<std::uint8_t, 64> prefix{};
    in_.read(reinterpret_cast<char *>(prefix.data()),
             checked_streamsize(prefix.size(), "record header"));
    const auto got = in_.gcount();

    if (got == 0 && in_.eof()) {
        return false;
    }
    if (got != static_cast<std::streamsize>(prefix.size())) {
        throw std::runtime_error("truncated record header at record index "
                                 + std::to_string(record_index_));
    }

    std::vector<std::uint8_t> bytes(prefix.begin(), prefix.end());
    Header h = parse_header(bytes);

    if (h.record_length > bytes.size()) {
        const std::size_t remaining = h.record_length - bytes.size();
        const std::size_t old_size = bytes.size();
        bytes.resize(h.record_length);
        in_.read(reinterpret_cast<char *>(bytes.data() + old_size),
                 checked_streamsize(remaining, "record body"));
        if (in_.gcount() != checked_streamsize(remaining, "record body")) {
            throw std::runtime_error("truncated record body at record index "
                                     + std::to_string(record_index_));
        }
    }

    // Cross-check the protocol's 3-bit packet-length index against Blockette
    // 1000. The protocol defines record bytes as 2^(index + 5).
    const std::size_t indexed_length = std::size_t{1}
                                       << (h.packet_length_index + 5u);
    if (indexed_length != h.record_length) {
        throw std::runtime_error(
            "packet-length index disagrees with Blockette 1000 at record index "
            + std::to_string(record_index_));
    }

    record.header = std::move(h);
    record.samples = decode_steim2(bytes, record.header);
    ++record_index_;
    return true;
}

double sample_rate_hz(std::int16_t factor, std::int16_t multiplier) {
    if (factor == 0 || multiplier == 0) {
        return 0.0;
    }

    double rate = factor > 0 ? static_cast<double>(factor)
                             : 1.0 / static_cast<double>(-factor);
    if (multiplier > 0) {
        rate *= static_cast<double>(multiplier);
    } else {
        rate /= static_cast<double>(-multiplier);
    }
    return rate;
}

double sample_rate_hz(const Header &header) {
    return sample_rate_hz(header.sample_rate_factor,
                          header.sample_rate_multiplier);
}

std::string format_btime(const BTime &t) {
    std::ostringstream oss;
    oss << std::setfill('0') << std::setw(4) << t.year << "-DOY" << std::setw(3)
        << t.day_of_year << 'T' << std::setw(2) << static_cast<unsigned>(t.hour)
        << ':' << std::setw(2) << static_cast<unsigned>(t.minute) << ':'
        << std::setw(2) << static_cast<unsigned>(t.second) << '.'
        << std::setw(4) << t.fraction_0001s << 'Z';
    return oss.str();
}

std::string dimension_name(Dimension d) {
    switch (d) {
        case Dimension::Dimensionless:
            return "dimensionless";
        case Dimension::Displacement:
            return "displacement";
        case Dimension::Velocity:
            return "velocity";
        case Dimension::Acceleration:
            return "acceleration";
    }
    return "unknown";
}

double count_to_physical(std::int32_t count, std::uint32_t sensitivity_raw) {
    if (sensitivity_raw == 0) {
        throw std::runtime_error("cannot convert count with zero sensitivity");
    }

    // The protocol stores the sensitivity as:
    //   actual sensitivity [count / SI-unit] * 100
    // Therefore the physical value is:
    //   count / (sensitivity_raw / 100)
    // = count * 100 / sensitivity_raw.
    return static_cast<double>(count) * 100.0
           / static_cast<double>(sensitivity_raw);
}

} // namespace qrest_data::tools::mseed
