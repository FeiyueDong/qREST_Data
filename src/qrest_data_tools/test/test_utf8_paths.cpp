/**
**            qREST - Quick Response Evaluation for Safety Tagging
**     Institute of Engineering Mechanics, China Earthquake Administration
**                 Copyright 2024 - 2026 QLab, Dong Feiyue
**                          All Rights Reserved.
** Project: qREST
** File: /src/qrest_data_tools/test/test_utf8_paths.cpp
** File Created: Wednesday, 28 October 2026
** Author: Codex
** Last Modified: Wednesday, 28 October 2026
** Modified By: Codex
*/
#include <qrest_data/external_reader.h>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <tuple>
#include "../../common/utf8_path.hpp"
#include "../core/channel_import.hpp"
#include "../core/dataset_draft.hpp"
#include "../core/text_matrix.hpp"
#include "../formats/tdms/tdms_reader.hpp"
using namespace qrest_data::tools;
using qrest_data::detail::path_from_utf8;
using qrest_data::detail::path_to_utf8;
namespace fs = std::filesystem;
namespace {
int checks{};
void check(bool condition, const char *message) {
    ++checks;
    if (!condition)
        throw std::runtime_error(message);
}
std::string read(const fs::path &p) {
    std::ifstream input(p, std::ios::binary);
    if (!input)
        throw std::runtime_error("fixture missing");
    return {std::istreambuf_iterator<char>(input), {}};
}
void write(const fs::path &p, const std::string &value) {
    std::ofstream output(p, std::ios::binary);
    output.write(value.data(), value.size());
    check(bool(output), "native path write");
}
} // namespace
int main() try {
    const auto temp =
        fs::temp_directory_path()
        / ("qrest-utf8-"
           + std::to_string(
               std::chrono::steady_clock::now().time_since_epoch().count()));
    struct Cleanup {
        fs::path path;
        ~Cleanup() {
            std::error_code ec;
            fs::remove_all(path, ec);
        }
    } cleanup{temp};
    const auto root = temp / fs::path(u8"中文 目录_😀") / fs::path(u8"楼层 一");
    fs::create_directories(root);
    check(path_from_utf8(path_to_utf8(root)) == root, "UTF8/native round trip");
    const fs::path fixture = "resource/test/format_readers";
    for (const auto &[format, source, suffix] :
         {std::tuple{"mseed", "standard_1s.bin", ".mseed"},
          std::tuple{"sit-dat", "sit_1s.dat", ".dat"},
          std::tuple{"sit-txt", "sit_1s.txt", ".txt"}}) {
        const auto directory = root / format;
        fs::create_directory(directory);
        const auto file = directory / (fs::path(u8"传感器 数据") += suffix);
        fs::copy_file(fixture / source, file);
        const auto direct = read_channel_import(path_to_utf8(file), format);
        const auto merged =
            read_channel_import(path_to_utf8(directory), format);
        qrest_external_dataset *sdk{};
        check(qrest_external_read(
                  path_to_utf8(directory).c_str(), format, nullptr, &sdk)
                      == 0
                  && sdk,
              "UTF8 public C reader boundary");
        std::unique_ptr<qrest_external_dataset, decltype(&qrest_external_free)>
            sdk_owner(sdk, qrest_external_free);
        check(qrest_external_channel_count(sdk) == 3
                  && qrest_external_sample_count(sdk) == 100,
              "C reader retains Unicode directory shape");
        check(direct.data.sample_count == 100 && direct.data.channel_count == 3,
              "Unicode format file read");
        check(merged.data.channel_sequential_data
                      == direct.data.channel_sequential_data
                  && merged.scales == direct.scales,
              "Unicode directory and nested file read agree");
    }
    auto metadata =
        qrest_data::Metadata::from_bytes(read(fixture / "metadata.json"));
    const auto json = root / fs::path(u8"元信息.json");
    write(json, metadata.to_bytes(2));
    std::vector<double> values(300);
    for (std::size_t i = 0; i < values.size(); ++i)
        values[i] = double(i) - 125.25;
    const auto text = root / fs::path(u8"波形 数据.txt");
    write_time_major_text_matrix(path_to_utf8(text), values, 3, 100, 17);
    check(validate_text_dataset(path_to_utf8(json), path_to_utf8(text)).ok(),
          "Unicode Metadata JSON and TXT validation");
    const auto numeric = read_channel_import(path_to_utf8(text), "text");
    check(numeric.data.channel_sequential_data == values,
          "Unicode numeric TXT inferred matrix");
    const auto packet = root / fs::path(u8"记录 数据.qrest");
    write_qrest_file(path_to_utf8(packet), metadata, values, 0, 1);
    check(validate_qrest_file(path_to_utf8(packet)).ok()
              && read_qrest_file(path_to_utf8(packet)).channel_sequential_data
                     == values,
          "Unicode qREST write and read");
    auto draft = DatasetDraft::from_file(packet);
    const auto saved = root / fs::path(u8"编辑 副本.qrest");
    draft.save_new(saved);
    check(DatasetDraft::from_file(saved).values() == values,
          "Unicode exclusive draft save and reopen");
    const auto hdf = root / fs::path(u8"存储 数据.h5");
    write_hdf5_dataset(path_to_utf8(hdf), metadata, values);
    qrest_data::Metadata loaded;
    check(load_hdf5_dataset(path_to_utf8(hdf), &loaded).channel_sequential_data
                  == values
              && loaded.to_bytes() == metadata.to_bytes(),
          "Unicode HDF5 native UTF8 filename contract");
    // Minimal valid metadata-only TDMS segment. This exercises the reader's
    // real file open/segment reads, not the separate acquisition calibration.
    std::string tdms_bytes = "TDSm";
    auto little = [&](std::uint64_t value, int count) {
        for (int i = 0; i < count; ++i)
            tdms_bytes.push_back(char(value >> (i * 8)));
    };
    little(2, 4);
    little(4713, 4);
    little(UINT64_MAX, 8);
    little(4, 8);
    little(0, 4);
    const auto tdms_file = root / fs::path(u8"采集 数据.tdms");
    write(tdms_file, tdms_bytes);
    tdms::Reader reader(path_to_utf8(tdms_file));
    tdms::Segment segment;
    check(reader.next(segment) && segment.version == 4713
              && segment.objects.empty() && !reader.next(segment),
          "Unicode TDMS native segment read");
    std::cout << "UTF8 path checks passed: " << checks << '\n';
    return 0;
} catch (const std::exception &e) {
    std::cerr << e.what() << '\n';
    return 1;
}
