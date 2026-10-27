#include "steim2.hpp"
#include <array>
#include <limits>
#include <sstream>
#include <stdexcept>

namespace qrest_data::tools::mseed {
namespace {
std::uint32_t be_u32(const std::uint8_t *p) {
    return (static_cast<std::uint32_t>(p[0]) << 24)
           | (static_cast<std::uint32_t>(p[1]) << 16)
           | (static_cast<std::uint32_t>(p[2]) << 8)
           | static_cast<std::uint32_t>(p[3]);
}

std::uint32_t le_u32(const std::uint8_t *p) {
    return (static_cast<std::uint32_t>(p[3]) << 24)
           | (static_cast<std::uint32_t>(p[2]) << 16)
           | (static_cast<std::uint32_t>(p[1]) << 8)
           | static_cast<std::uint32_t>(p[0]);
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

} // namespace

std::vector<std::int32_t> decode_steim2(std::span<const std::uint8_t> record,
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

} // namespace qrest_data::tools::mseed
