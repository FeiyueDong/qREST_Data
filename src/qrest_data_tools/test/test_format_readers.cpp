#include <qrest_data/external_reader.h>
#include <algorithm>
#include <bit>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <nlohmann/json.hpp>
#include <stdexcept>
#include "../core/external_import.hpp"
#include "../core/qrest_file.hpp"
#include "../formats/mseed/modified_mseed.hpp"
#include "../formats/mseed/modified_mseed_export.hpp"
#include "../formats/sit/sit_dat_reader.hpp"

#ifndef QREST_DATA_PROJECT_DIR
#define QREST_DATA_PROJECT_DIR "."
#endif

using namespace qrest_data::tools;
namespace fs = std::filesystem;
namespace {
std::size_t checks = 0;
void check(bool ok, const std::string &message) {
    ++checks;
    if (!ok)
        throw std::runtime_error(message);
}
std::vector<std::uint8_t> bytes(const fs::path &path) {
    std::ifstream in(path, std::ios::binary);
    if (!in)
        throw std::runtime_error("missing fixture: " + path.string());
    return {std::istreambuf_iterator<char>(in), {}};
}
void save(const fs::path &path, const std::vector<std::uint8_t> &b) {
    std::ofstream out(path, std::ios::binary);
    out.write(reinterpret_cast<const char *>(b.data()), b.size());
}
template <class F>
void rejects(F f, const std::string &fragment) {
    try {
        f();
    } catch (const std::exception &e) {
        check(std::string(e.what()).find(fragment) != std::string::npos,
              "wrong error: " + std::string(e.what()));
        return;
    }
    throw std::runtime_error("expected rejection: " + fragment);
}
void compare(const fs::path &dat,
             const fs::path &mseed,
             const fs::path &txt,
             std::size_t n) {
    MseedImportOptions counts;
    counts.output_counts = true;
    SitDatImportOptions dat_counts;
    dat_counts.output_counts = true;
    const auto raw = load_mseed_dataset(mseed.string(), counts);
    const auto d_raw = load_sit_dat_dataset(dat.string(), dat_counts);
    const auto d = load_sit_dat_dataset(dat.string());
    const auto t = load_sit_txt_dataset(txt.string());
    check(raw.source_format == "miniseed-v2"
              && raw.representation == "RawCounts",
          "standard raw representation");
    check(raw.sample_count == n && d.sample_count == n && t.sample_count == n,
          "sample counts");
    check(raw.channel_sequential_data == d_raw.channel_sequential_data,
          "all DAT/MiniSEED counts exactly equal");
    check(raw.channel_labels == std::vector<std::string>{"EIN", "EIE", "EIZ"},
          "binary source order");
    check(t.channel_labels == std::vector<std::string>{"UD", "NS", "EW"},
          "TXT source order");
    check(raw.channels[0].axis_hint == "North"
              && t.channels[0].axis_hint == "Vertical",
          "axes");
    check(raw.start_time_us == t.start_time_us
              && d.start_time_us == raw.start_time_us,
          "UTC alignment");
    check(raw.sample_rate_hz == 100 && t.sample_rate_hz == 100, "100 Hz");
    check(d.channel_sequential_data[0] == 0.019
              && d.channel_sequential_data[n] == -0.008
              && d.channel_sequential_data[2 * n] == 0.008,
          "physical first values");
    check(d.channels[0].sensitivity == 100000
              && d.channels[0].sensitivity_origin == "native-extension",
          "native calibration");
    check(!raw.channels[0].sensitivity
              && raw.channel_measurands[0] == "RawCounts",
          "no fabricated physical metadata");
    double max_error = 0;
    const std::size_t order[]{1, 2, 0};
    for (std::size_t c = 0; c < 3; ++c)
        for (std::size_t i = 0; i < n; ++i)
            max_error = std::max(
                max_error,
                std::abs(d.channel_sequential_data[c * n + i]
                         - t.channel_sequential_data[order[c] * n + i]));
    check(max_error <= 1e-12, "all DAT Gal/TXT physical values");
    MseedImportOptions physical;
    physical.sensitivity = 100000;
    physical.quantity = "Acceleration";
    physical.output_unit = MotionUnit::Centimeter;
    const auto m = load_mseed_dataset(mseed.string(), physical);
    check(m.channel_sequential_data == d.channel_sequential_data,
          "explicit MiniSEED physical conversion");
    const auto dat_bytes = bytes(dat), seed_bytes = bytes(mseed);
    std::size_t matched = 0;
    for (std::size_t block = 0; block < n / 100; ++block)
        for (std::size_t c = 0; c < 3; ++c) {
            const auto a = 512 + block * 788 + 8 + c * 260 + 4 + 64,
                       b = (block * 3 + c) * 512 + 64;
            check(std::equal(dat_bytes.begin() + a,
                             dat_bytes.begin() + a + 192,
                             seed_bytes.begin() + b),
                  "shared compressed payload");
            ++matched;
        }
    const auto j = nlohmann::json::parse(external_dataset_summary_json(t));
    check(j["last_sample_time_us"].get<std::int64_t>()
              == *t.start_time_us + std::int64_t(n - 1) * 10000,
          "last sample time");
    std::cout << "Equivalent input: " << 3 * n << " counts, " << matched
              << " payloads, max Gal error=" << max_error << '\n';
}
// Synthetic MiniSEED 2, with a legal Steim-2 ramp independent of site fixtures.
std::vector<std::uint8_t> timing_record(double rate,
                                        std::size_t record_index,
                                        bool microseconds = false,
                                        bool actual_rate = false,
                                        std::int64_t shift_us = 0,
                                        char component = 'Z',
                                        std::int64_t phase_us = 0) {
    std::vector<std::uint8_t> data(512, 0);
    const auto put16 = [&](std::size_t p, std::uint16_t v) {
        data[p] = std::uint8_t(v >> 8);
        data[p + 1] = std::uint8_t(v);
    };
    const auto put32 = [&](std::size_t p, std::uint32_t v) {
        for (int i = 3; i >= 0; --i) {
            data[p + i] = std::uint8_t(v);
            v >>= 8;
        }
    };
    std::copy_n("000001D TST01  HHZXX", 20, data.begin());
    data[17] = std::uint8_t(component);
    const auto epoch = mseed::btime_unix_us({2026, 295, 14, 0, 0, 0});
    const long double exact = record_index * 100 * 1000000.0L / rate + phase_us;
    const auto resolution = microseconds ? 1 : 100;
    const auto time =
        epoch + std::llround(exact / resolution) * resolution + shift_us;
    // Round the BTime base separately so Blockette 1001 exercises signed
    // offsets.
    const auto base =
        std::int64_t(std::llround((time - epoch) / 100.0L)) * 100 + epoch;
    const auto bt = mseed::btime_from_unix_us(base);
    put16(20, bt.year);
    put16(22, bt.day_of_year);
    data[24] = bt.hour;
    data[25] = bt.minute;
    data[26] = bt.second;
    put16(28, bt.fraction_0001s);
    put16(30, 100);
    put16(32, actual_rate ? 200 : std::uint16_t(rate));
    put16(34, 1);
    data[39] = std::uint8_t(1 + actual_rate + microseconds);
    put16(44, 128);
    put16(46, 48);
    put16(48, 1000);
    data[52] = 11;
    data[53] = 1;
    data[54] = 9;
    std::size_t next = 56;
    if (actual_rate) {
        put16(50, std::uint16_t(next));
        put16(next, 100);
        put32(next + 4, std::bit_cast<std::uint32_t>(float(rate)));
        if (microseconds)
            put16(next + 2, std::uint16_t(next + 12));
        next += 12;
    } else if (microseconds)
        put16(50, std::uint16_t(next));
    if (microseconds) {
        put16(next, 1001);
        data[next + 4] = 100;
        data[next + 5] = std::uint8_t(std::int8_t(time - base));
    }
    const auto first = std::uint32_t(record_index * 100);
    put32(132, first);
    put32(136, first + 99);
    // d0=1 (ignored), then 99 increments of one. Extra differences are padding.
    for (std::size_t frame = 0; frame < 2; ++frame) {
        std::uint32_t control = 0;
        for (std::size_t word = frame == 0 ? 3 : 1; word < 16; ++word) {
            control |= std::uint32_t{1} << (30 - 2 * word);
            put32(128 + frame * 64 + word * 4, 0x01010101);
        }
        put32(128 + frame * 64, control);
    }
    return data;
}
std::vector<mseed::Record>
timing_records(double rate,
               const std::vector<std::size_t> &indices,
               bool precise = false,
               bool actual_rate = false,
               char component = 'Z',
               std::int64_t phase_us = 0) {
    std::vector<mseed::Record> records;
    for (const auto i : indices)
        records.push_back(mseed::parse_record(timing_record(
            rate, i, precise, actual_rate, 0, component, phase_us)));
    return records;
}
void timing_precision(const fs::path &temp) {
    mseed::LoadOptions strict;
    strict.gap_policy = mseed::GapPolicy::Error;
    for (double rate : {75.0, 120.0}) {
        auto records =
            timing_records(rate, {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11});
        const auto groups = mseed::assemble_channel_groups(records, strict);
        check(groups.size() == 1 && groups[0].sample_count() == 1200,
              "fractional-duration record assembly");
        const auto &ch = groups[0].channels[0];
        check(ch.missing_sample_count == 0 && ch.record_count == 12,
              "no fabricated quantization gaps");
        check(ch.start_time_us == records.front().header.start_time_us,
              "first source timestamp retained");
        check(ch.last_record_time.year == records.back().header.start_time.year
                  && ch.last_record_time.fraction_0001s
                         == records.back().header.start_time.fraction_0001s,
              "last record timestamp retained");
        bool values = true;
        for (std::size_t i = 0; i < ch.values.size(); ++i)
            values = values && ch.values[i] == double(i);
        check(values, "fractional-duration values and order");
    }
    for (double rate : {75.0, 120.0}) {
        auto truncated = timing_records(rate, {0, 1, 2, 3, 4, 5, 6, 7, 8, 9});
        for (std::size_t i = 0; i < truncated.size(); ++i) {
            const auto exact = i * 100 * 1000000.0L / rate;
            const auto shift = std::int64_t(std::floor(exact / 100)) * 100
                               - std::llround(exact / 100) * 100;
            truncated[i] = mseed::parse_record(
                timing_record(rate, i, false, false, shift));
        }
        const auto result = mseed::assemble_channel_groups(truncated, strict);
        check(result[0].sample_count() == 1000
                  && result[0].channels[0].missing_sample_count == 0
                  && result[0].channels[0].values.back() == 999,
              "BTime truncation as well as nearest rounding");
    }
    // Halfway ties can legitimately round in opposite directions. The phase
    // intersection is closed so both coarse and microsecond ties remain valid.
    for (bool precise : {false, true}) {
        const double rate = precise ? 512 : 128;
        auto ties = timing_records(rate, {0, 1, 2, 3, 4, 5}, precise);
        const auto q = precise ? 1 : 100;
        for (std::size_t i = 0; i < ties.size(); ++i) {
            const auto exact = i * 100 * 1000000.0L / rate / q;
            const auto slot = std::int64_t(std::floor(exact));
            const auto even = slot + (exact - slot == 0.5L && slot % 2 ? 1 : 0);
            const auto shift = (even - std::llround(exact)) * q;
            ties[i] = mseed::parse_record(
                timing_record(rate, i, precise, false, shift));
        }
        const auto g = mseed::assemble_channel_groups(ties, strict);
        check(g[0].sample_count() == 600
                  && g[0].channels[0].missing_sample_count == 0
                  && g[0].channels[0].values.back() == 599,
              "round-to-even halfway timestamp quantization");
    }
    for (double rate : {75.5, 120.125}) {
        const auto records = timing_records(rate, {0, 1, 2, 3}, false, true);
        check(records[0].header.sample_rate_factor == 200
                  && mseed::sample_rate_hz(records[0].header) == rate,
              "Blockette 100 rate overrides fixed factor");
        const auto g = mseed::assemble_channel_groups(records, strict);
        check(g[0].sample_rate_hz == rate && g[0].sample_count() == 400
                  && g[0].channels[0].values.back() == 399
                  && g[0].channels[0].missing_sample_count == 0,
              "Blockette 100 fractional-rate values and coverage");
    }
    check(timing_records(75, {0})[0].header.time_resolution_us() == 100,
          "BTime encoding resolution");
    for (double rate : {75.0, 120.0}) {
        const auto records =
            timing_records(rate, {0, 1, 2, 3, 4, 5}, true, false, 'Z', 25);
        const auto g = mseed::assemble_channel_groups(records, strict);
        const auto &ch = g[0].channels[0];
        check(records[0].header.has_blockette_1001
                  && records[0].header.time_resolution_us() == 1,
              "Blockette 1001 precision provenance");
        check(ch.start_time_us
                      == mseed::btime_unix_us({2026, 295, 14, 0, 0, 0}) + 25
                  && ch.last_record_time_us
                         == records.back().header.start_time_us,
              "signed microsecond offsets preserved");
        check(g[0].sample_count() == 600 && ch.record_count == 6
                  && ch.missing_sample_count == 0 && ch.values.front() == 0
                  && ch.values.back() == 599,
              "precise fractional-duration samples and endpoints");
        const auto combined = timing_records(rate, {0, 1, 2}, true, true);
        check(mseed::assemble_channel_groups(combined, strict)[0].sample_count()
                  == 300,
              "Blockette 100 plus 1001 chain");
        auto bad = timing_records(rate, {0, 1, 2}, true);
        bad[1] = mseed::parse_record(timing_record(rate, 1, true, false, 20));
        rejects([&] { mseed::assemble_channel_groups(bad); }, "residual_us");
        // A coarse first record cannot relax later precise/precise comparisons.
        bad[0] = timing_records(rate, {0})[0];
        rejects([&] { mseed::assemble_channel_groups(bad); }, "sampling grid");
        const auto missing = timing_records(rate, {0, 2, 3});
        rejects([&] { mseed::assemble_channel_groups(missing, strict); },
                "missing_samples=100");
        const auto filled = mseed::assemble_channel_groups(missing);
        const auto &gap = filled[0].channels[0];
        check(filled[0].sample_count() == 400 && gap.valid_sample_count == 300
                  && gap.missing_sample_count == 100 && gap.gaps.size() == 1,
              "whole missing record retained");
        bool values = true;
        for (std::size_t i = 0; i < 400; ++i)
            values = values
                     && (i >= 100 && i < 200 ? std::isnan(gap.values[i])
                                             : gap.values[i] == double(i));
        check(values
                  && gap.last_record_time_us
                         == missing.back().header.start_time_us,
              "NaN gap values and later real positions");
        // A single sample is also a real gap; it cannot disappear into
        // tolerance.
        auto one = timing_records(rate, {0, 1});
        one[1] = mseed::parse_record(timing_record(
            rate, 1, false, false, std::llround(1000000.0 / rate)));
        const auto single = mseed::assemble_channel_groups(one);
        check(single[0].sample_count() == 201
                  && single[0].channels[0].missing_sample_count == 1
                  && std::isnan(single[0].channels[0].values[100])
                  && single[0].channels[0].values[101] == 100,
              "one-sample gap remains visible");
        rejects([&] { mseed::assemble_channel_groups(one, strict); },
                "missing_samples=1");
        auto overlap = timing_records(rate, {0, 1});
        overlap.push_back(overlap.back());
        rejects([&] { mseed::assemble_channel_groups(overlap); }, "overlap");
        auto early = timing_records(rate, {0, 1});
        early[1] = mseed::parse_record(timing_record(
            rate, 1, false, false, -std::llround(1000000.0 / rate)));
        rejects([&] { mseed::assemble_channel_groups(early); }, "overlap");
        mseed::LoadOptions ignore;
        ignore.gap_policy = mseed::GapPolicy::Ignore;
        const auto concatenated =
            mseed::assemble_channel_groups(missing, ignore);
        check(concatenated[0].sample_count() == 300
                  && concatenated[0].channels[0].missing_sample_count == 0
                  && concatenated[0].channels[0].values[100] == 200,
              "explicit ignore retains concatenation behavior");
        check(mseed::assemble_channel_groups(overlap, ignore)[0].sample_count()
                  == 300,
              "overlap allowed only by explicit ignore");
        auto ordered = timing_records(rate, {0, 1, 2, 3});
        const auto shuffled = std::vector<mseed::Record>{
            ordered[3], ordered[1], ordered[0], ordered[2]};
        const auto a = mseed::assemble_channel_groups(ordered, strict),
                   b = mseed::assemble_channel_groups(shuffled, strict);
        check(a[0].channels[0].values == b[0].channels[0].values
                  && a[0].start_time_us == b[0].start_time_us
                  && a[0].channels[0].last_record_time_us
                         == b[0].channels[0].last_record_time_us,
              "out-of-order record reconstruction");
        // Align shifted channel coverage on the same precision-aware grid.
        auto channels = timing_records(rate, {0, 1, 2, 3}, false, false, 'N');
        const auto other = timing_records(rate, {1, 2}, false, false, 'E');
        channels.insert(channels.end(), other.begin(), other.end());
        const auto aligned = mseed::assemble_channel_groups(channels);
        const auto &east = aligned[0].channels[1];
        check(aligned[0].sample_count() == 400
                  && east.leading_missing_sample_count == 100
                  && east.trailing_missing_sample_count == 100
                  && east.missing_sample_count == 200,
              "cross-channel fractional leading/trailing gaps");
        check(east.start_time_us == aligned[0].start_time_us
                  && std::isnan(east.values[0]) && east.values[100] == 100
                  && east.values[299] == 299 && std::isnan(east.values[399]),
              "cross-channel aligned values");
        rejects([&] { mseed::assemble_channel_groups(channels, strict); },
                "coverage differs");
    }
    auto phased = timing_records(75, {0, 1, 2}, true, false, 'N');
    const auto shifted = timing_records(75, {0, 1, 2}, true, false, 'E', 20);
    phased.insert(phased.end(), shifted.begin(), shifted.end());
    rejects([&] { mseed::assemble_channel_groups(phased); }, "sampling grid");
    // Precision gained in later records still constrains cross-channel
    // alignment.
    auto mixed = timing_records(75, {0}, false, false, 'N');
    mixed.push_back(timing_records(75, {1}, true, false, 'N')[0]);
    mixed.push_back(timing_records(75, {0}, false, false, 'E')[0]);
    mixed.push_back(timing_records(75, {1}, true, false, 'E', 20)[0]);
    rejects([&] { mseed::assemble_channel_groups(mixed); },
            "cross-channel timestamp quantization");
    // An observable clock step cannot be hidden by coarse BTime tolerance.
    auto stepped = timing_records(100, {0, 1, 2});
    stepped[1] = mseed::parse_record(timing_record(100, 1, false, false, 200));
    stepped[2] = mseed::parse_record(timing_record(100, 2, false, false, 200));
    rejects([&] { mseed::assemble_channel_groups(stepped); }, "sampling grid");
    // Coarse phase variations cannot accumulate into a drifting time grid.
    auto drift = timing_records(75, {0, 1, 2, 3, 4, 5});
    drift[1] = mseed::parse_record(timing_record(75, 1, false, false, 100));
    drift[5] = mseed::parse_record(timing_record(75, 5, false, false, -100));
    rejects([&] { mseed::assemble_channel_groups(drift); }, "quantization");
    for (double rate : {5000.0, 10000.0}) {
        const auto too_coarse = timing_records(rate, {0, 1, 2});
        rejects([&] { mseed::assemble_channel_groups(too_coarse); },
                "insufficient timing precision");
        check(mseed::assemble_channel_groups(
                  timing_records(rate, {0, 1, 2}, true), strict)[0]
                      .sample_count()
                  == 300,
              "microsecond timestamp supports higher rates");
    }
    rejects(
        [&] {
            mseed::assemble_channel_groups(
                timing_records(500000, {0, 1, 2}, true, true));
        },
        "insufficient timing precision");
    auto bad = timing_record(75, 0, true, true);
    bad[60] = 0x7f;
    bad[61] = 0xc0;
    bad[62] = bad[63] = 0;
    rejects([&] { mseed::parse_record(bad); }, "Blockette 100 sample rate");
    bad = timing_record(75, 0, true);
    bad[39] = 3;
    bad[59] = 64;
    bad[64] = 3;
    bad[65] = 233;
    rejects([&] { mseed::parse_record(bad); }, "duplicate Blockette 1001");
    bad = timing_record(75, 0, false, true);
    bad[39] = 3;
    bad[59] = 68;
    bad[68] = 0;
    bad[69] = 100;
    rejects([&] { mseed::parse_record(bad); }, "duplicate Blockette 100");
    // The same generated stream traverses the Reader, shared C ABI and qREST.
    std::vector<std::uint8_t> stream;
    for (std::size_t i = 0; i < 4; ++i) {
        const auto data = timing_record(75, i);
        stream.insert(stream.end(), data.begin(), data.end());
    }
    const auto source = (temp / "timing.mseed").string();
    save(source, stream);
    const auto external = read_external_dataset(source, "mseed");
    check(external.sample_count == 400 && external.sample_rate_hz == 75
              && external.representation == "RawCounts",
          "public reader fractional sampling rate");
    qrest_external_dataset *handle = nullptr;
    check(qrest_external_read(source.c_str(), "mseed", nullptr, &handle) == 0,
          "C ABI fractional stream read");
    const auto meta_bytes =
        bytes(fs::path(QREST_DATA_PROJECT_DIR)
              / "resource/test/format_readers/metadata.json");
    auto metadata = qrest_data::Metadata::from_bytes(
        std::string(meta_bytes.begin(), meta_bytes.end()));
    metadata.Units[0] = "count";
    metadata.DataInfo.DT = 1.0 / 75;
    metadata.DataInfo.NPTS = 400;
    metadata.InstrumentInfo.ChannelNum = 1;
    metadata.InstrumentInfo.Channels.resize(1);
    auto &channel = metadata.InstrumentInfo.Channels[0];
    channel.ChannelID = "HHZ";
    channel.Measurand = "RawCounts";
    const auto json = metadata.to_bytes();
    const auto path = (temp / "timing.qrest").string();
    const auto status = qrest_external_write_qrest(
        handle, json.c_str(), nullptr, 0, path.c_str(), 1, 1);
    qrest_external_free(handle);
    check(status == 0, qrest_external_last_error());
    const auto back = read_qrest_file(path);
    check(back.metadata.DataInfo.NPTS == 400
              && back.metadata.DataInfo.DT == 1.0 / 75
              && back.metadata.DataInfo.StartTime == metadata.DataInfo.StartTime
              && back.metadata.Units[0] == "count"
              && back.metadata.InstrumentInfo.Channels[0].Scale == 1
              && back.channel_sequential_data
                     == external.channel_sequential_data,
          "fractional-rate C ABI qREST values/DT/NPTS/StartTime/unit/Scale");
}

void malformed(const fs::path &fixtures, const fs::path &temp) {
    const auto seed = bytes(fixtures / "standard_1s.bin");
    auto changed = seed;
    changed[52] = 10;
    rejects([&] { mseed::parse_record(std::span(changed).first(512)); },
            "unsupported MiniSEED encoding: 10");
    changed = seed;
    changed[45] = 255;
    rejects([&] { mseed::parse_record(std::span(changed).first(512)); },
            "offset");
    changed = seed;
    changed[75] ^= 1;
    rejects([&] { mseed::parse_record(std::span(changed).first(512)); },
            "constant mismatch");
    rejects([&] { mseed::parse_record(std::span(seed).first(511)); },
            "truncated");
    changed = seed;
    changed[51] = 48;
    rejects([&] { mseed::parse_record(std::span(changed).first(512)); },
            "Blockette");
    // Move Blockette 1000 and enlarge the record: no fixed 48/512 assumptions.
    changed.assign(seed.begin(), seed.begin() + 512);
    changed.resize(1024);
    std::copy_n(seed.begin() + 48, 8, changed.begin() + 56);
    changed[47] = 56;
    changed[62] = 10;
    const auto large = mseed::parse_record(changed);
    check(large.samples.front() == 19 && large.header.record_length == 1024,
          "variable length and Blockette offset");
    // Little-endian fixed header and payload are independent.
    changed.assign(seed.begin(), seed.begin() + 512);
    for (int p : {20, 22, 28, 30, 32, 34, 44, 46, 48, 50})
        std::swap(changed[p], changed[p + 1]);
    std::reverse(changed.begin() + 40, changed.begin() + 44);
    check(mseed::parse_record(changed).samples == large.samples,
          "little-endian fixed header");
    changed.assign(seed.begin(), seed.begin() + 512);
    changed[39] = 2;
    changed[51] = 56;
    changed[56] = 3;
    changed[57] = 233;
    changed[58] = 0;
    changed[59] = 0;
    changed[61] = 25;
    changed[43] =
        1; // unapplied 100us fixed-header correction plus 25us Blockette 1001
    const auto precise = mseed::parse_record(changed);
    check(precise.header.start_time_us == large.header.start_time_us + 125,
          "microsecond and fixed time corrections");
    // Time sorting, gaps, overlap and grid validation share the same assembler.
    auto a = mseed::parse_record(std::span(seed).first(512));
    auto b = a;
    b.header.start_time_us += 2000000;
    b.header.start_time = mseed::btime_from_unix_us(b.header.start_time_us);
    const auto groups = mseed::assemble_channel_groups({b, a});
    check(groups[0].sample_count() == 300
              && groups[0].channels[0].missing_sample_count == 100,
          "time sorted NaN gap");
    check(std::isnan(groups[0].channels[0].values[100]),
          "gap never filled with zero");
    mseed::LoadOptions strict;
    strict.gap_policy = mseed::GapPolicy::Error;
    rejects([&] { mseed::assemble_channel_groups({a, b}, strict); },
            "discontinuity");
    rejects([&] { mseed::assemble_channel_groups({a, a}); }, "overlap");
    b.header.start_time_us += 5000;
    rejects([&] { mseed::assemble_channel_groups({a, b}); }, "sampling grid");
    // Corrupt the sample count, integration range, encoding, and identity
    // independently.
    changed = seed;
    changed[30] = 255;
    changed[31] = 255;
    rejects([&] { mseed::parse_record(std::span(changed).first(512)); },
            "not enough");
    changed = seed;
    changed[30] = 0;
    changed[31] = 2;
    changed[64] = 1;
    changed[65] = changed[66] = changed[67] = 0;
    changed[68] = 0x7f;
    changed[69] = changed[70] = changed[71] = 0xff;
    changed[76] = 0;
    changed[77] = 1;
    changed[78] = changed[79] = 0;
    rejects([&] { mseed::parse_record(std::span(changed).first(512)); },
            "overflow");
    changed = seed;
    changed[0] = 'M';
    changed[1] = 'S';
    changed[2] = 3;
    rejects([&] { mseed::parse_record(std::span(changed).first(40)); },
            "MiniSEED 3");
    changed = seed;
    changed[52] = 3;
    changed[45] = 56;
    rejects([&] { mseed::parse_record(std::span(changed).first(512)); },
            "unsupported MiniSEED encoding: 3");
    changed.assign(seed.begin(), seed.begin() + 512);
    for (std::size_t i = 64; i < 512; i += 4)
        std::reverse(changed.begin() + i, changed.begin() + i + 4);
    changed[53] = 0;
    check(mseed::parse_record(changed).samples == large.samples,
          "little-endian Steim payload");
    changed.assign(seed.begin(), seed.begin() + 512);
    std::copy_n("ABCDE", 5, changed.begin() + 8);
    std::copy_n("HH1", 3, changed.begin() + 15);
    changed[0] = '0';
    changed[1] = '0';
    changed[2] = '0';
    changed[3] = '0';
    changed[4] = '0';
    changed[5] = '1';
    const auto renamed = mseed::parse_record(changed);
    check(renamed.samples == large.samples
              && external_axis_hint(renamed.header.channel) == "Unknown",
          "no station/sequence/channel name assumption");
    auto dat = bytes(fixtures / "sit_1s.dat");
    dat.pop_back();
    save(temp / "bad.dat", dat);
    rejects([&] { load_sit_dat_dataset((temp / "bad.dat").string()); },
            "block 0");
    dat = bytes(fixtures / "sit_1s.dat");
    dat[520] = 2;
    save(temp / "bad.dat", dat);
    rejects([&] { load_sit_dat_dataset((temp / "bad.dat").string()); },
            "marker");
    dat = bytes(fixtures / "sit_1s.dat");
    dat[516] = 0;
    save(temp / "bad.dat", dat);
    rejects([&] { load_sit_dat_dataset((temp / "bad.dat").string()); },
            "record");
    auto text = bytes(fixtures / "sit_1s.txt");
    text[0] = 'X';
    save(temp / "bad.txt", text);
    rejects([&] { load_sit_txt_dataset((temp / "bad.txt").string()); },
            "line 1");
    const std::string utf8 =
        "\xef\xbb\xbfUD向\tNS向\tEW向\t时间\r\n+0.008\t0.019\t-0.008\t2026/10/"
        "22 22:00:00.000\r\n0\t0\t0\t2026/10/22 22:00:00.010\r\n";
    save(temp / "utf8.txt", {utf8.begin(), utf8.end()});
    check(load_sit_txt_dataset((temp / "utf8.txt").string()).sample_rate_hz
              == 100,
          "UTF8 BOM CRLF signed numbers");
    auto broken = utf8;
    broken.replace(broken.find(".010"), 4, ".000");
    save(temp / "bad.txt", {broken.begin(), broken.end()});
    rejects([&] { load_sit_txt_dataset((temp / "bad.txt").string()); },
            "line 3");
    broken = utf8 + "\n";
    save(temp / "bad.txt", {broken.begin(), broken.end()});
    rejects([&] { load_sit_txt_dataset((temp / "bad.txt").string()); },
            "line 4");
    save(temp / "x_SRate_200_test.txt", {utf8.begin(), utf8.end()});
    rejects(
        [&] { load_sit_txt_dataset((temp / "x_SRate_200_test.txt").string()); },
        "SRate");
    rejects(
        [&] {
            load_sit_txt_dataset((fixtures / "sit_1s.txt").string(),
                                 {MotionUnit::SI, "UTC"});
        },
        "timezone");
    auto opt = MseedImportOptions{};
    opt.require_physical = true;
    rejects(
        [&] {
            load_mseed_dataset((fixtures / "standard_1s.bin").string(), opt);
        },
        "sensitivity");
    opt.sensitivity = 100000;
    rejects(
        [&] {
            load_mseed_dataset((fixtures / "standard_1s.bin").string(), opt);
        },
        "quantity");
    const auto arbitrary = (fixtures / "standard_1s.bin").string();
    check(read_external_dataset(arbitrary, "mseed").representation
              == "RawCounts",
          "explicit format ignores suffix");
    rejects(
        [&] {
            read_external_dataset(arbitrary, "mseed", "{\"group_index\":1.5}");
        },
        "nonnegative integer");
    rejects(
        [&] {
            read_external_dataset(arbitrary,
                                  "mseed",
                                  "{\"sensitivity\":100000,\"quantity\":"
                                  "\"Velocity\",\"unit\":\"m/s2\"}");
        },
        "unit conflicts");
    auto modified_bytes = bytes(fixtures / "sit_1s.dat");
    rejects(
        [&] {
            mseed::parse_record(std::span(modified_bytes).subspan(524, 56));
        },
        "extension layout");
    auto modified = sit::read_dat_records((fixtures / "sit_1s.dat").string());
    auto next = modified[0];
    next.header.start_time_us += 1000000;
    next.header.sensitivity++;
    rejects([&] { mseed::assemble_channel_groups({modified[0], next}); },
            "sensitivity changed");
}
bool values_near(const std::vector<double> &a, const std::vector<double> &b) {
    if (a.size() != b.size())
        return false;
    for (std::size_t i = 0; i < a.size(); ++i)
        if (std::abs(a[i] - b[i]) > 1e-12)
            return false;
    return true;
}
void sdk_and_mapping(const fs::path &fixtures, const fs::path &temp) {
    const auto d = load_sit_dat_dataset((fixtures / "sit_1s.dat").string());
    const auto metadata_bytes = bytes(fixtures / "metadata.json");
    auto metadata = qrest_data::Metadata::from_bytes(
        std::string(metadata_bytes.begin(), metadata_bytes.end()));
    metadata.InstrumentInfo.ChannelNum = 3;
    metadata.InstrumentInfo.Channels.resize(3);
    metadata.DataInfo.NPTS = 100;
    metadata.DataInfo.DT = 0.01;
    metadata.DataInfo.StartTime = "2026-10-22T14:00:00.000Z";
    metadata.Units = {"cm", "s"};
    const char *labels[]{"NS", "EW", "UD"};
    for (std::size_t i = 0; i < 3; ++i) {
        auto &c = metadata.InstrumentInfo.Channels[i];
        c.ChannelNo = int(i + 1);
        c.ChannelID = labels[i];
        c.Measurand = "Acceleration";
        c.Scale = 1;
    }
    require_external_dataset_compatibility(d, metadata);
    const auto t = load_sit_txt_dataset((fixtures / "sit_1s.txt").string());
    rejects(
        [&] {
            apply_external_channel_mapping(
                t, metadata, make_sequential_channel_mapping(t, metadata));
        },
        "direction conflict");
    const std::vector<ExternalChannelMapping> map{{0, 2}, {1, 0}, {2, 1}};
    check(values_near(apply_external_channel_mapping(t, metadata, map)
                          .channel_sequential_data,
                      d.channel_sequential_data),
          "explicit direction mapping");
    auto wrong = metadata;
    wrong.Units[0] = "m";
    rejects([&] { require_external_dataset_compatibility(d, wrong); }, "unit");
    wrong = metadata;
    wrong.DataInfo.StartTime = "2026-10-22T22:00:00.000Z";
    rejects([&] { require_external_dataset_compatibility(d, wrong); },
            "start time");
    wrong = metadata;
    wrong.InstrumentInfo.Channels[0].Scale = 100;
    rejects([&] { require_external_dataset_compatibility(d, wrong); },
            "Scale=1");
    const auto raw =
        load_mseed_dataset((fixtures / "standard_1s.bin").string());
    rejects([&] { require_external_dataset_compatibility(raw, metadata); },
            "RawCounts");
    qrest_external_dataset *handle = nullptr;
    const auto txt = (fixtures / "sit_1s.txt").string();
    check(qrest_external_read(txt.c_str(), "sit-txt", nullptr, &handle) == 0,
          "shared C read");
    check(qrest_external_sample_count(handle) == 100
              && qrest_external_channel_count(handle) == 3,
          "C dimensions");
    check(qrest_external_values(handle)[0] == 0.008, "borrowed C values");
    check(nlohmann::json::parse(qrest_external_summary(handle))["source_format"]
              == "sit-txt",
          "C report");
    const auto json = metadata.to_bytes();
    const std::size_t targets[]{2, 0, 1};
    const auto out = (temp / "mapped.qrest").string();
    check(qrest_external_write_qrest(
              handle, json.c_str(), targets, 3, out.c_str(), 1, 1)
              == 0,
          qrest_external_last_error());
    const auto back = read_qrest_file(out);
    check(values_near(back.channel_sequential_data, d.channel_sequential_data),
          "shared C qrest roundtrip all values");
    const auto bad = (temp / "invalid.qrest").string();
    check(qrest_external_write_qrest(
              handle, json.c_str(), nullptr, 0, bad.c_str(), 1, 1)
              != 0,
          "C invalid map rejected");
    check(!fs::exists(bad), "invalid mapping creates no output");
    qrest_external_free(handle);
    handle = nullptr;
    check(qrest_external_read(
              txt.c_str(), "sit-txt", "{\"counts\":true}", &handle)
                  != 0
              && !handle,
          "invalid TXT options");
    check(qrest_external_read(
              txt.c_str(), "sit-txt", "{\"unknown\":true}", &handle)
                  != 0
              && !handle,
          "unknown options rejected");
    check(qrest_external_read(nullptr, "sit-txt", nullptr, &handle) != 0
              && !handle,
          "C null args");
    const auto seed = (temp / "A.mseed");
    save(seed, bytes(fixtures / "standard_1s.bin"));
    save(temp / "B.mseed", bytes(fixtures / "standard_1s.bin"));
    rejects([&] { read_external_dataset(temp.string(), "mseed"); },
            "duplicate source");
}
} // namespace
int main(int argc, char **argv) {
    const fs::path root = QREST_DATA_PROJECT_DIR,
                   fixtures = root / "resource/test/format_readers";
    const auto temp = fs::temp_directory_path() / "qrest_format_reader_tests";
    fs::create_directories(temp);
    try {
        compare(fixtures / "sit_1s.dat",
                fixtures / "standard_1s.bin",
                fixtures / "sit_1s.txt",
                100);
        timing_precision(temp);
        malformed(fixtures, temp);
        sdk_and_mapping(fixtures, temp);
        if (argc > 1 && std::string(argv[1]) == "--full") {
            const auto example = root / "resource/example_data";
            compare(example / "HBSY0012026102222.dat",
                    example / "HBSY0012026102222_dat.mseed",
                    example
                        / "HBSY001_20261022_220000_SRate_100_SF_10000000_Gal_"
                          "dat.txt",
                    360000);
        }
        std::cout << "Format reader checks passed: " << checks << '\n';
        fs::remove_all(temp);
        return 0;
    } catch (const std::exception &e) {
        fs::remove_all(temp);
        std::cerr << e.what() << '\n';
        return 1;
    }
}
