#pragma once

#include <cstddef>
#include <cstdint>
#include <fstream>
#include <string>
#include <vector>

namespace qrest_data::tools::mseed {

enum class Dimension : std::uint8_t {
    Dimensionless = 0,
    Displacement = 1,
    Velocity = 2,
    Acceleration = 3,
};

struct BTime {
    std::uint16_t year{};
    std::uint16_t day_of_year{};
    std::uint8_t hour{};
    std::uint8_t minute{};
    std::uint8_t second{};
    std::uint16_t fraction_0001s{}; // 0.0001 s units
};

struct Header {
    std::string waveform_id;            // wc / wt / ws
    std::uint32_t packet_word{};        // raw 4-byte combined field
    std::uint8_t packet_length_index{}; // high 3 bits
    std::uint32_t sequence{};           // low 29 bits

    char quality_indicator{};
    char reserved_char{};
    std::string station;
    std::string location;
    std::string channel;
    std::string network;

    BTime start_time;
    std::uint16_t sample_count{};
    std::int16_t sample_rate_factor{};
    std::int16_t sample_rate_multiplier{};
    std::uint8_t activity_flags{};
    std::uint8_t io_clock_flags{};
    std::uint8_t data_quality_flags{};
    std::uint8_t blockette_count{};
    std::int32_t time_correction{};
    std::uint16_t data_offset{};
    std::uint16_t first_blockette_offset{};

    // Blockette 1000
    std::uint16_t blockette_type{};
    std::uint16_t next_blockette_offset{};
    std::uint8_t encoding{};
    std::uint8_t word_order{}; // 1 = big endian, 0 = little endian
    std::uint8_t record_length_exponent{};
    std::uint8_t blockette_reserved{};

    // Protocol-specific 8-byte extension after Blockette 1000
    std::string channel_order;     // e.g. NEZ
    std::uint8_t extension_byte{}; // documented as reserved; sample file
                                   // contains 0x76 ('v')
    std::uint32_t dimension_sensitivity_raw{};
    Dimension dimension{Dimension::Dimensionless};
    std::uint32_t sensitivity{}; // low 30 bits

    std::size_t record_length{};
};

struct Record {
    Header header;
    std::vector<std::int32_t> samples; // raw counts after Steim-2 decompression
};

class Reader {
public:
    explicit Reader(const std::string &path);

    // Returns false only for a clean EOF before the next record.
    // Throws std::runtime_error on malformed/truncated records.
    bool next(Record &record);

    std::size_t record_index() const noexcept { return record_index_; }

private:
    std::ifstream in_;
    std::size_t record_index_{};
};

// Standard SEED sample-rate interpretation.
double sample_rate_hz(std::int16_t factor, std::int16_t multiplier);
double sample_rate_hz(const Header &header);

std::string format_btime(const BTime &time);
std::string dimension_name(Dimension d);

// Optional physical conversion. The low 30-bit sensitivity field stores
// (actual sensitivity in count / SI-unit) * 100. Therefore this helper uses
// physical_value = count * 100 / sensitivity_raw.
double count_to_physical(std::int32_t count, std::uint32_t sensitivity_raw);

} // namespace qrest_data::tools::mseed
