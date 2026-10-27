#include "sit_dat_reader.hpp"
#include <algorithm>
#include <array>
#include <fstream>
#include <stdexcept>

namespace qrest_data::tools::sit {
namespace {
std::uint32_t le32(const std::uint8_t *p) {
    return p[0] | (std::uint32_t(p[1]) << 8) | (std::uint32_t(p[2]) << 16)
           | (std::uint32_t(p[3]) << 24);
}
} // namespace
std::vector<mseed::Record> read_dat_records(const std::string &path) {
    std::ifstream in(path, std::ios::binary);
    std::array<char, 512> header{};
    if (!in.read(header.data(), header.size())
        || std::string(header.data(), 16) != "!SIT-4S Zip File")
        throw std::runtime_error("unrecognized/truncated SIT-4S DAT header "
                                 "(512-byte layout profile)");
    std::vector<mseed::Record> records;
    std::vector<std::string> labels;
    std::uint32_t previous_counter{};
    std::int64_t previous_time{};
    for (std::size_t block = 0;; ++block) {
        const auto offset = in.tellg();
        std::array<std::uint8_t, 8> prefix{};
        in.read(reinterpret_cast<char *>(prefix.data()), 8);
        if (!in.gcount() && in.eof())
            break;
        try {
            if (in.gcount() != 8)
                throw std::runtime_error("truncated block prefix");
            const auto counter = le32(prefix.data()),
                       length = le32(prefix.data() + 4);
            // The observed profile stores the payload length in the second
            // control word.
            if (!length || length > 1024 * 1024)
                throw std::runtime_error("unsupported DAT block length");
            if (block && counter != previous_counter + 1)
                throw std::runtime_error("DAT block counter discontinuity");
            std::vector<std::uint8_t> payload(length);
            if (!in.read(reinterpret_cast<char *>(payload.data()), length))
                throw std::runtime_error("truncated DAT block payload");
            std::vector<std::string> current_labels;
            std::int64_t block_time{};
            double rate{};
            for (std::size_t pos = 0; pos < payload.size();) {
                if (payload.size() - pos < 68
                    || le32(payload.data() + pos) != 1)
                    throw std::runtime_error(
                        "unsupported DAT record marker/boundary");
                pos += 4;
                const auto h = mseed::parse_record_header(
                    std::span(payload).subspan(pos, 64));
                if (h.standard_v2 || h.record_length > payload.size() - pos)
                    throw std::runtime_error(
                        "unsupported/truncated DAT inner record layout");
                auto record = mseed::parse_record(
                    std::span(payload).subspan(pos, h.record_length));
                const auto id = h.network + "." + h.station + "." + h.location
                                + "." + h.channel;
                if (std::find(current_labels.begin(), current_labels.end(), id)
                    != current_labels.end())
                    throw std::runtime_error("duplicate DAT channel in block");
                if (current_labels.empty()) {
                    block_time = h.start_time_us;
                    rate = mseed::sample_rate_hz(h);
                }
                if (h.start_time_us != block_time
                    || mseed::sample_rate_hz(h) != rate
                    || rate != h.sample_count)
                    throw std::runtime_error("DAT profile requires synchronous "
                                             "one-second channel records");
                current_labels.push_back(id);
                records.push_back(std::move(record));
                pos += h.record_length;
            }
            if (current_labels.empty() || (block && current_labels != labels))
                throw std::runtime_error("DAT channel layout changed");
            if (block && block_time != previous_time + 1000000)
                throw std::runtime_error("DAT block time discontinuity");
            labels = std::move(current_labels);
            previous_time = block_time;
            previous_counter = counter;
        } catch (const std::exception &e) {
            throw std::runtime_error(
                "SIT DAT block " + std::to_string(block) + " at byte "
                + std::to_string(static_cast<std::streamoff>(offset)) + ": "
                + e.what());
        }
    }
    if (records.empty())
        throw std::runtime_error("SIT DAT contains no waveform blocks");
    return records;
}
} // namespace qrest_data::tools::sit
