#include <qrest_data/external_reader.h>
#include <algorithm>
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
