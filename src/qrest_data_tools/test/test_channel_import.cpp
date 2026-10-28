/**
**            qREST - Quick Response Evaluation for Safety Tagging
**     Institute of Engineering Mechanics, China Earthquake Administration
**                 Copyright 2024 - 2026 QLab, Dong Feiyue
**                          All Rights Reserved.
** Project: qREST
** File: /src/qrest_data_tools/test/test_channel_import.cpp
** File Created: Wednesday, 28 October 2026
** Author: Codex
** Last Modified: Wednesday, 28 October 2026
** Modified By: Codex
*/
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include "../core/channel_import.hpp"
#include "../core/dataset_draft.hpp"
#include "../core/text_matrix.hpp"
using namespace qrest_data::tools;
namespace fs = std::filesystem;
namespace {
int checks{};
void check(bool ok, const char *message) {
    ++checks;
    if (!ok)
        throw std::runtime_error(message);
}
void rejects(auto f) {
    bool rejected = false;
    try {
        f();
    } catch (const std::exception &) {
        rejected = true;
    }
    check(rejected, "expected rejection");
}
void write(const fs::path &p, const std::string &value) {
    std::ofstream(p) << value;
}
} // namespace
int main() try {
    const auto tmp = fs::temp_directory_path() / "qrest-channel-import-tests";
    fs::create_directories(tmp);
    struct Cleanup {
        fs::path p;
        ~Cleanup() {
            std::error_code ec;
            fs::remove_all(p, ec);
        }
    } cleanup{tmp};
    const auto txt = tmp / "matrix.txt";
    write(txt, "\xef\xbb\xbf+1e0, 2;3\r\n4\t5 6\r\n");
    const auto matrix = read_channel_text_matrix(txt.string(), false);
    check(
        matrix.channels == 3 && matrix.samples == 2
            && matrix.values == std::vector<double>{1, 4, 2, 5, 3, 6},
        "newline matrix, mixed delimiters, CRLF, BOM and scientific notation");
    const auto transposed = read_channel_text_matrix(txt.string(), true);
    check(transposed.channels == 2 && transposed.samples == 3
              && transposed.values == std::vector<double>{1, 2, 3, 4, 5, 6},
          "transpose keeps exact per-line channels");
    for (const auto *bad : {"1,2\n3\n",
                            "1,,2\n",
                            "1,2,\n",
                            "1; ;2\n",
                            "1 NaN\n",
                            "1 x\n",
                            "\n"}) {
        write(txt, bad);
        rejects([&] { read_channel_import(txt.string(), "text"); });
    }
    write(txt, "1,2\n3,4\n");
    auto numeric = read_channel_import(txt.string(), "text");
    DatasetDraft draft;
    draft.import_channels(
        numeric, DatasetDraft::ImportAction::Replace, {0.01, -2});
    check(draft.data_sample_rate() == 0 && !draft.data_start_time()
              && draft.metadata().DataInfo.StartTime.empty(),
          "TXT never fabricates rate or time");
    check(draft.values() == std::vector<double>{.01, .03, -4, -8}
              && !draft.validate().ok(),
          "unknown sampling remains editable but cannot be published");
    DatasetDraft::Channel c;
    c.ChannelID = "N";
    c.DeviceType = "Test";
    c.Measurand = "Acceleration";
    c.Scale = 1;
    draft.add_channel(c);
    c.ChannelID = "E";
    draft.add_channel(c);
    draft.bind_unassigned_sequentially();
    auto metadata = draft.metadata();
    metadata.DataInfo.DT = .01;
    metadata.DataInfo.StartTime = "2026-10-22T14:00:00Z";
    metadata.BuildingInfo.StructuralFootprint.Shape = "Rectangular";
    metadata.BuildingInfo.StructuralFootprint.Parameters.Length = 10;
    metadata.BuildingInfo.StructuralFootprint.Parameters.Width = 8;
    metadata.BuildingInfo.Elevation = {0, 3};
    draft.replace_metadata(metadata);
    check(draft.data_sample_rate() == 100 && draft.validate().ok(),
          "global metadata supplies TXT time axis without fictitious source "
          "conflicts");
    const auto saved = tmp / "matrix.qrest";
    draft.save_new(saved);
    auto reloaded = DatasetDraft::from_file(saved);
    check(reloaded.values() == draft.values()
              && reloaded.metadata().Extension["ChannelImports"][0]["columns"]
                                              [0]["scale_to_mps2"]
                     == .01,
          "physical values and applied Scale provenance survive save");
    const auto before = draft.values_handle();
    const auto meta_before = draft.metadata().to_bytes();
    rejects([&] {
        draft.import_channels(
            numeric, DatasetDraft::ImportAction::Replace, {0, 1});
    });
    check(draft.values_handle() == before
              && draft.metadata().to_bytes() == meta_before,
          "bad gain is atomic");
    draft.import_channels(
        numeric, DatasetDraft::ImportAction::Append, numeric.scales);
    check(draft.data_column_count() == 4 && draft.unbound_data_count() == 2
              && draft.data_sample_rate() == 100,
          "TXT append uses global axis and keeps new columns unbound");
    const fs::path fixtures = "resource/test/format_readers";
    const auto dat =
        read_channel_import((fixtures / "sit_1s.dat").string(), "sit-dat");
    const auto seed =
        read_channel_import((fixtures / "standard_1s.bin").string(), "mseed");
    const auto sit =
        read_channel_import((fixtures / "sit_1s.txt").string(), "sit-txt");
    check(dat.data.channel_sequential_data[0] == 19 && dat.scales[0] == 1e-5
              && seed.scales[0] == 1,
          "native/default count calibration has one explicit Scale");
    check(sit.scales[0] == .01
              && sit.data.channel_labels
                     == std::vector<std::string>{"UD", "NS", "EW"},
          "SIT Gal default and original channel order");
    DatasetDraft binary;
    binary.import_channels(
        dat, DatasetDraft::ImportAction::Replace, dat.scales);
    check(binary.metadata().DataInfo.DT == .01
              && binary.metadata().DataInfo.NPTS == 100
              && binary.metadata().DataInfo.StartTime
                     == "2026-10-22T14:00:00.000Z",
          "native sampling adopted without separate flags");
    check(std::abs(binary.values()[0] - .00019) < 1e-18,
          "native counts convert to SI exactly once");
    DatasetDraft standard;
    standard.import_channels(
        seed, DatasetDraft::ImportAction::Replace, {1e-5, 1e-5, 1e-5});
    check(standard.values() == binary.values(),
          "user Scale converts standard MiniSEED identically to native DAT");
    DatasetDraft text;
    text.import_channels(sit, DatasetDraft::ImportAction::Replace, sit.scales);
    const std::size_t order[]{1, 2, 0};
    for (std::size_t ch = 0; ch < 3; ++ch)
        for (std::size_t t = 0; t < 100; ++t)
            check(std::abs(text.values()[order[ch] * 100 + t]
                           - binary.values()[ch * 100 + t])
                      < 1e-16,
                  "all SIT samples agree after explicit axis reorder");
    auto cm = metadata;
    cm.Units[0] = "cm";
    cm.DataInfo.NPTS = 100;
    for (auto &channel : cm.InstrumentInfo.Channels)
        channel.Scale = 9;
    DatasetDraft centimeter;
    centimeter.import_metadata(cm);
    centimeter.import_channels(
        dat, DatasetDraft::ImportAction::Replace, dat.scales);
    check(centimeter.metadata().Units[0] == "cm"
              && centimeter.metadata()
                         .BuildingInfo.StructuralFootprint.Parameters.Length
                     == 10
              && centimeter.metadata().InstrumentInfo.Channels[0].Scale == 1
              && std::abs(centimeter.values()[0] - .019) < 1e-16,
          "existing geometry unit is preserved and only imported bound scales "
          "reset");
    auto conflict = dat;
    conflict.data.sample_rate_hz = 200;
    auto unchanged = binary.values_handle();
    rejects([&] {
        binary.import_channels(
            conflict, DatasetDraft::ImportAction::Append, conflict.scales);
    });
    check(binary.values_handle() == unchanged,
          "native append conflicts do not mutate the draft");
    auto velocity = dat;
    velocity.data.channel_measurands.assign(3, "Velocity");
    check(!channel_import_error(velocity).empty(),
          "a multiplier cannot convert velocity to acceleration");
    auto mixed = binary;
    auto velocity_metadata = metadata;
    for (auto &channel : velocity_metadata.InstrumentInfo.Channels)
        channel.Measurand = "Velocity";
    auto velocity_values = std::make_shared<const std::vector<double>>(
        std::vector<double>{1, 2, 3, 4});
    mixed = DatasetDraft::from_values(velocity_metadata, velocity_values, {});
    rejects([&] {
        mixed.import_channels(
            numeric, DatasetDraft::ImportAction::Append, numeric.scales);
    });
    auto micro = dat;
    micro.data.start_time_us = *micro.data.start_time_us + 1;
    check(!channel_import_error(micro).empty(),
          "microseconds are never truncated");
    auto hdfmeta = cm;
    hdfmeta.InstrumentInfo.Channels.resize(3, c);
    hdfmeta.InstrumentInfo.ChannelNum = 3;
    for (auto &channel : hdfmeta.InstrumentInfo.Channels)
        channel.Scale = 2;
    for (std::size_t i = 0; i < hdfmeta.InstrumentInfo.Channels.size(); ++i) {
        hdfmeta.InstrumentInfo.Channels[i].ChannelID =
            "H" + std::to_string(i + 1);
    }
    normalize_draft_metadata(hdfmeta);
    const auto hdf = tmp / "stored.h5";
    write_hdf5_dataset(hdf.string(), hdfmeta, dat.data.channel_sequential_data);
    const auto hdfdata = read_channel_import(hdf.string(), "hdf5");
    check(hdfdata.scales == std::vector<double>{.02, .02, .02},
          "HDF5 stored Scale and units become effective SI factors");
    DatasetDraft imported;
    imported.import_channels(
        hdfdata, DatasetDraft::ImportAction::Replace, hdfdata.scales);
    check(imported.values()[0] == .38, "HDF5 conversion is applied once");
    std::cout << "Channel import checks passed: " << checks << '\n';
    return 0;
} catch (const std::exception &e) {
    std::cerr << e.what() << '\n';
    return 1;
}
