/**
**            qREST - Quick Response Evaluation for Safety Tagging
**     Institute of Engineering Mechanics, China Earthquake Administration
**                 Copyright 2024 - 2026 QLab, Dong Feiyue
**                          All Rights Reserved.
** Project: qREST
** File: /src/qrest_data_tools/test/test_dataset_draft.cpp
** File Created: Sunday, 18 October 2026
** Author: Codex
** Last Modified: Sunday, 18 October 2026
** Modified By: Codex
*/
#include <atomic>
#include <chrono>
#include <fstream>
#include <iostream>
#include <limits>
#include <thread>
#include "../core/dataset_draft.hpp"
using namespace qrest_data::tools;
namespace fs = std::filesystem;
int checks{};
void check(bool ok, const char *label) {
    ++checks;
    if (!ok)
        throw std::runtime_error(label);
}
template <class F>
void rejects(F f, const char *label) {
    bool rejected = false;
    try {
        f();
    } catch (const std::exception &) {
        rejected = true;
    }
    check(rejected, label);
}
int main() try {
    auto root =
        fs::temp_directory_path()
        / ("qrest-draft-"
           + std::to_string(
               std::chrono::steady_clock::now().time_since_epoch().count()));
    fs::create_directories(root);
    struct Cleanup {
        fs::path p;
        ~Cleanup() {
            std::error_code ec;
            fs::remove_all(p, ec);
        }
    } cleanup{root};
    qrest_data::Metadata m;
    m.BuildingInfo.StructuralFootprint.Shape = "Rectangular";
    m.BuildingInfo.StructuralFootprint.Parameters.Length = 10;
    m.BuildingInfo.StructuralFootprint.Parameters.Width = 8;
    m.BuildingInfo.Elevation = {0, 3};
    m.DataInfo.NPTS = 3;
    m.DataInfo.DT = .01;
    m.DataInfo.StartTime = "2026-10-18T00:00:00Z";
    DatasetDraft::Channel c;
    c.ChannelID = "UNKNOWN";
    c.DeviceType = "Test";
    c.Measurand = "Acceleration";
    c.Scale = .5;
    m.InstrumentInfo.Channels = {c, c};
    m.InstrumentInfo.Channels[1].Scale = 2;
    m.Extension["Keep"] = "custom";
    auto samples = std::make_shared<const std::vector<double>>(
        std::vector<double>{1, 2, 3, 10, 20, 30});
    auto d = DatasetDraft::from_values(m, samples, {});
    check(d.validate().ok(), "valid input");
    auto metadata = d.metadata();
    metadata.BuildingInfo.ProjectName = "edited";
    d.replace_metadata(metadata);
    check(d.values_handle() == samples, "metadata shares samples");
    check(m.BuildingInfo.ProjectName.empty(), "source unchanged");
    d.select_channels({1, 0});
    check(d.values()[0] == 10
              && d.metadata().InstrumentInfo.Channels[0].Scale == 2,
          "channel reorder");
    d.duplicate_channel(0);
    check(d.values()[6] == 10 && d.metadata().InstrumentInfo.ChannelNum == 3,
          "duplicate waveform");
    d.select_channels({2, 1});
    check(d.values()[0] == 10 && d.values()[3] == 1, "delete channel");
    for (const auto *shape : {"Circular", "Polygon"}) {
        auto geometry = d.metadata();
        geometry.BuildingInfo.StructuralFootprint.Shape = shape;
        geometry.BuildingInfo.StructuralFootprint.Parameters.Radius = 5;
        geometry.BuildingInfo.StructuralFootprint.Parameters.Corners = {
            {0, 0}, {3, 0}, {1, 4}};
        d.replace_metadata(geometry);
        d.save_new(root / (std::string(shape) + ".qrest"));
        auto read =
            DatasetDraft::from_file(root / (std::string(shape) + ".qrest"));
        check(read.metadata().BuildingInfo.StructuralFootprint.Shape == shape,
              "shape roundtrip");
    }
    auto before = d.values_handle();
    auto json = d.metadata().to_bytes();
    rejects([&] { d.select_channels({0, 0}); }, "duplicate order rejected");
    rejects([&] { d.add_channel(c); }, "no invented zeros");
    check(d.values_handle() == before && d.metadata().to_bytes() == json,
          "transaction rollback");
    auto output = root / fs::path(u8"新数据.qrest");
    d.save_new(output);
    auto read = DatasetDraft::from_file(output);
    check(read.values() == d.values() && read.metadata().to_bytes() == json,
          "raw roundtrip");
    rejects([&] { d.save_new(output); }, "existing file protected");
    rejects(
        [&] {
            d.save_new(root / "verify-reject.qrest", [](const auto &) {
                throw std::runtime_error("consumer verifier failure");
            });
        },
        "consumer verifier rejection");
    check(!fs::exists(root / "verify-reject.qrest"),
          "no file published before consumer verification");
    std::error_code ec;
    fs::create_symlink(root / "absent", root / "link.qrest", ec);
    if (!ec)
        rejects([&] { d.save_new(root / "link.qrest"); }, "symlink protected");
    std::atomic<int> saved{}, failed{};
    auto race = [&] {
        try {
            d.save_new(root / "race.qrest");
            ++saved;
        } catch (...) {
            ++failed;
        }
    };
    std::thread a(race), b(race);
    a.join();
    b.join();
    check(saved == 1 && failed == 1, "exclusive publication race");
    check(DatasetDraft::from_file(root / "race.qrest").values() == d.values(),
          "race output valid");
    DraftSource src;
    src.normalized_physical = true;
    src.snapshot_id = "snapshot";
    auto physical = DatasetDraft::from_values(m, samples, src);
    physical.source_removed();
    samples.reset();
    check(physical.metadata().InstrumentInfo.Channels[0].Scale == 1
              && physical.metadata().InstrumentInfo.Channels[1].Scale == 1,
          "Scale=1 physical input");
    check(physical.values()[5] == 30 && physical.source().removed,
          "removed source ownership");
    physical.save_new(root / "physical.qrest");
    check(DatasetDraft::from_file(root / "physical.qrest").values()
              == physical.values(),
          "physical roundtrip");
    ExternalDataset e;
    e.channel_count = 2;
    e.sample_count = 3;
    e.sample_rate_hz = 100;
    e.channel_sequential_data = {7, 8, 9, 70, 80, 90};
    auto imp = d.metadata();
    d.import_data(e, {1, 0}, DatasetDraft::ImportAction::Replace, imp, true);
    check(d.values()[0] == 70 && d.values()[3] == 7
              && d.metadata().InstrumentInfo.Channels[0].Scale == 1,
          "mapped replace");
    e.distance_unit = "cm";
    rejects(
        [&] {
            d.import_data(
                e, {0, 1}, DatasetDraft::ImportAction::Replace, imp, true);
        },
        "unit conflict");
    e.distance_unit.clear();
    e.channel_measurands = {"Velocity", "Velocity"};
    rejects(
        [&] {
            d.import_data(
                e, {0, 1}, DatasetDraft::ImportAction::Replace, imp, true);
        },
        "measurand conflict");
    e.channel_measurands.clear();
    before = d.values_handle();
    json = d.metadata().to_bytes();
    e.sample_count = 2;
    rejects(
        [&] {
            d.import_data(
                e, {0, 1}, DatasetDraft::ImportAction::Replace, imp, true);
        },
        "import mismatch");
    check(d.values_handle() == before && d.metadata().to_bytes() == json,
          "import rollback");
    e.sample_count = 3;
    imp = d.metadata();
    imp.InstrumentInfo.Channels.push_back(c);
    d.import_data(e, {0}, DatasetDraft::ImportAction::Append, imp, true);
    check(d.values()[6] == 7 && d.metadata().InstrumentInfo.ChannelNum == 3,
          "append channel");
    before = d.values_handle();
    imp = d.metadata();
    imp.InstrumentInfo.Channels.push_back(c);
    imp.DataInfo.StartTime = "2026-10-19T00:00:00Z";
    rejects(
        [&] {
            d.import_data(
                e, {0}, DatasetDraft::ImportAction::Append, imp, true);
        },
        "append time conflict");
    check(d.values_handle() == before, "append rollback");
    auto bad = std::make_shared<const std::vector<double>>(
        std::vector<double>{std::numeric_limits<double>::quiet_NaN()});
    rejects([&] { DatasetDraft::from_values(d.metadata(), bad, {}); },
            "invalid input");
    auto extreme = d.metadata();
    extreme.DataInfo.DT = 1e-300;
    rejects([&] { d.apply_json(extreme.to_bytes()); },
            "extreme JSON sampling rate rejected");
    auto invalid_draft = d;
    invalid_draft.replace_metadata(extreme);
    check(!invalid_draft.validate().ok(),
          "extreme structured sampling rate rejected");
    rejects([&] { d.apply_json("{}"); }, "invalid JSON");
    rejects([&] { d.save_new(root / "absent" / "fail.qrest"); },
            "write failure");
    for (const auto &entry : fs::directory_iterator(root))
        check(!entry.path().filename().string().starts_with(".qrest-editor-"),
              "temporary cleanup");
    std::cout << checks << " dataset draft checks passed\n";
} catch (const std::exception &e) {
    std::cerr << e.what() << "\n";
    return 1;
}
