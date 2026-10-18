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
#include "../core/text_matrix.hpp"
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
    std::ofstream(root / "source.csv") << "1,2,3\n10,20,30\n";
    check(read_text_matrix((root / "source.csv").string(), 2, 3, true, ',')
              == *samples,
          "channel-major CSV import");
    std::ofstream(root / "corrupt.txt") << "1 junk\n";
    rejects(
        [&] { read_text_matrix((root / "corrupt.txt").string(), 1, 1, false); },
        "corrupt trailing token rejected");
    auto original = DatasetDraft::from_values(m, samples, {});
    check(original.validate().ok(), "B17 raw source valid");
    auto d = original;
    auto metadata = d.metadata();
    metadata.BuildingInfo.ProjectName = "edited";
    d.replace_metadata(metadata);
    check(d.values_handle() == samples, "metadata shares sample buffer");
    auto ids = d.metadata_uids();
    d.reorder_metadata_channels({ids[1], ids[0]});
    check(d.values_handle() == samples
              && d.binding(d.data_columns()[0].uid) == ids[0],
          "B06 reorder keeps buffer and identity");
    check(d.project_formal_output().values[0] == 10,
          "reordered formal projection");
    d.duplicate_channel(0);
    d.add_channel(c);
    check(d.data_column_count() == 2 && d.values_handle() == samples
              && !d.column_for(d.metadata_uids()[2]),
          "B05 metadata additions have no waveforms");
    d.reorder_metadata_channels({ids[0]});
    check(d.data_column_count() == 2 && d.unbound_data_count() == 1
              && d.values_handle() == samples,
          "B04 delete unbinds without touching samples");
    rejects([&] { d.reorder_metadata_channels({ids[0], ids[0]}); },
            "duplicate UID transaction rejected");
    check(d.metadata_uids().size() == 1, "failed transaction unchanged");
    auto before = d.values_handle();
    d.import_metadata(original.metadata());
    check(d.unbound_data_count() == 2 && d.values_handle() == before,
          "B11 full Metadata replacement clears bindings");
    d.bind_unassigned_sequentially();
    auto a = d.data_columns()[0].uid, b = d.data_columns()[1].uid;
    auto first = d.metadata_uids()[0], second = d.metadata_uids()[1];
    rejects([&] { d.bind_column(a, second); }, "B07 busy target rejected");
    check(d.binding(a) == first && d.binding(b) == second,
          "busy target rollback");
    d.swap_bindings(a, b);
    check(d.binding(a) == second && d.values_handle() == before,
          "B08 swap bindings only");
    d.unbind_column(a);
    check(d.available_channels(b).size() == 2, "B09 available channels update");
    d.bind_column(a, second);
    d.swap_bindings(a, b);
    ExternalDataset e;
    e.channel_count = 20;
    e.sample_count = 3;
    e.sample_rate_hz = 100;
    for (int i = 0; i < 60; ++i)
        e.channel_sequential_data.push_back(i + 1);
    DatasetDraft many;
    auto eighteen = m;
    eighteen.InstrumentInfo.Channels.resize(18, c);
    many.import_metadata(eighteen);
    many.import_waveforms(e, DatasetDraft::ImportAction::Replace, false);
    check(many.data_column_count() == 20 && many.unbound_data_count() == 2,
          "B01 initial min-count binding");
    check(many.validate().ok() && !many.validate().warnings.empty(),
          "B13 extra data is a warning");
    auto full = many.values_handle();
    auto projected = many.project_formal_output();
    check(projected.values.size() == 54 && projected.omitted_columns == 2
              && many.values_handle() == full,
          "B13 projection leaves all 20 real columns");
    many.unbind_column(many.data_columns()[0].uid);
    check(!many.validate().ok(), "B14 missing allocation fails Final");
    e.channel_count = 15;
    e.channel_sequential_data.resize(45);
    many.import_waveforms(e, DatasetDraft::ImportAction::Replace, false);
    check(!many.validate().ok() && many.validate(ValidationMode::Draft).ok(),
          "B02 fewer columns valid only as draft");
    DatasetDraft data_first;
    data_first.import_waveforms(e, DatasetDraft::ImportAction::Replace, false);
    check(data_first.unbound_data_count() == 15, "data without Metadata");
    data_first.import_metadata(eighteen);
    check(data_first.unbound_data_count() == 0
              && data_first.column_for(data_first.metadata_uids()[0]) == 0,
          "B03 first coexistence binds");
    auto oldid = many.data_columns()[0].uid;
    many.import_waveforms(e, DatasetDraft::ImportAction::Replace, false);
    check(many.data_columns()[0].uid != oldid && many.unbound_data_count() == 0,
          "B12 new data identities and initial bindings");
    auto oldbinding = many.binding(many.data_columns()[0].uid);
    many.import_waveforms(e, DatasetDraft::ImportAction::Append, false);
    check(many.data_column_count() == 30 && many.unbound_data_count() == 15
              && many.binding(many.data_columns()[0].uid) == oldbinding,
          "B10 append keeps previous bindings, new columns unbound");
    auto badappend = e;
    badappend.sample_rate_hz = 50;
    auto intact = many.values_handle();
    rejects(
        [&] {
            many.import_waveforms(
                badappend, DatasetDraft::ImportAction::Append, false);
        },
        "append source facts checked");
    check(many.values_handle() == intact, "append rollback");
    DatasetDraft projection;
    e.channel_count = 5;
    e.channel_sequential_data.resize(15);
    projection.import_waveforms(e, DatasetDraft::ImportAction::Replace, false);
    auto three = m;
    three.InstrumentInfo.Channels.resize(3, c);
    projection.import_metadata(three);
    for (auto col : projection.data_columns())
        projection.unbind_column(col.uid);
    auto cols = projection.data_columns();
    auto uids = projection.metadata_uids();
    projection.bind_column(cols[2].uid, uids[0]);
    projection.bind_column(cols[3].uid, uids[1]);
    projection.bind_column(cols[0].uid, uids[2]);
    auto mapped = projection.project_formal_output();
    check(mapped.values == std::vector<double>({7, 8, 9, 10, 11, 12, 1, 2, 3}),
          "B15 non-contiguous D3,D4,D1 projection");
    auto output = root / fs::path(u8"新数据.qrest");
    d.save_new(output);
    auto read = DatasetDraft::from_file(output);
    check(read.values() == d.project_formal_output().values,
          "formal raw roundtrip");
    projection.save_new(root / "mapped.qrest");
    check(DatasetDraft::from_file(root / "mapped.qrest").values()
                  == mapped.values
              && projection.data_column_count() == 5,
          "B15 projected save preserves source waves");
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
    std::thread writer_a(race), writer_b(race);
    writer_a.join();
    writer_b.join();
    check(saved == 1 && failed == 1, "exclusive publication race");
    check(DatasetDraft::from_file(root / "race.qrest").values() == d.values(),
          "race output valid");
    DraftSource src;
    src.normalized_physical = true;
    src.snapshot_id = "snapshot";
    auto physical = DatasetDraft::from_values(m, samples, src);
    physical.source_removed();
    check(physical.metadata().InstrumentInfo.Channels[0].Scale == 1
              && physical.values_handle() == samples,
          "B17 QANS physical Scale1 ownership");
    auto mismatch = physical.metadata();
    mismatch.InstrumentInfo.Channels[0].Scale = 2;
    physical.replace_metadata(mismatch);
    check(!physical.validate().ok(), "B16 physical Scale mismatch rejected");
    mismatch.InstrumentInfo.Channels[0].Scale = 1;
    mismatch.DataInfo.NPTS = 4;
    physical.replace_metadata(mismatch);
    check(!physical.validate().ok(), "B16 sample count mismatch");
    mismatch.DataInfo.NPTS = 3;
    mismatch.Units[0] = "cm";
    physical.replace_metadata(mismatch);
    check(!physical.validate().ok(), "B16 known unit mismatch");
    physical = DatasetDraft::from_values(m, samples, src);
    physical.save_new(root / "physical.qrest");
    check(DatasetDraft::from_file(root / "physical.qrest").values()
              == physical.values(),
          "B17 physical roundtrip");
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
