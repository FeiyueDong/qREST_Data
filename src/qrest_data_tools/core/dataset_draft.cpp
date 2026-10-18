/**
**            qREST - Quick Response Evaluation for Safety Tagging
**     Institute of Engineering Mechanics, China Earthquake Administration
**                 Copyright 2024 - 2026 QLab, Dong Feiyue
**                          All Rights Reserved.
** Project: qREST
** File: /src/qrest_data_tools/core/dataset_draft.cpp
** File Created: Sunday, 18 October 2026
** Author: Codex
** Last Modified: Sunday, 18 October 2026
** Modified By: Codex
*/
#include "dataset_draft.hpp"
#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <limits>
#include <set>
#include <stdexcept>

namespace qrest_data::tools {
namespace {
std::string utf8(const std::filesystem::path &p) {
    auto s = p.u8string();
    return {s.begin(), s.end()};
}
void require(bool condition, const std::string &message) {
    if (!condition)
        throw std::runtime_error(message);
}
void require_report(const ValidationReport &r) {
    if (!r.ok()) {
        std::string s;
        for (const auto &e : r.errors)
            s += e + "\n";
        throw std::runtime_error(s);
    }
}
void check_values(const Metadata &m, const std::vector<double> &v) {
    require(m.DataInfo.NPTS > 0 && !m.InstrumentInfo.Channels.empty(),
            "Missing samples/channels");
    require(v.size()
                == std::size_t(m.DataInfo.NPTS)
                       * m.InstrumentInfo.Channels.size(),
            "Sample/column count mismatch");
    for (auto x : v)
        require(std::isfinite(x),
                "Samples must be finite (NaN/Inf cannot be saved)");
}
bool close(double a, double b) {
    return std::abs(a - b) <= 1e-10 * std::max({1., std::abs(a), std::abs(b)});
}
struct TempDirectory {
    std::filesystem::path path;
    ~TempDirectory() {
        std::error_code ec;
        if (!path.empty())
            std::filesystem::remove_all(path, ec);
    }
};
} // namespace
void normalize_draft_metadata(Metadata &m) {
    auto &i = m.InstrumentInfo;
    i.ChannelNum = static_cast<int>(i.Channels.size());
    for (std::size_t c = 0; c < i.Channels.size(); ++c)
        i.Channels[c].ChannelNo = static_cast<int>(c + 1);
    m.BuildingInfo.ElevationNum =
        static_cast<int>(m.BuildingInfo.Elevation.size());
    auto &f = m.BuildingInfo.StructuralFootprint;
    auto &b = f.BoundingBox;
    if (f.Shape == "Rectangular") {
        b.MinX = -f.Parameters.Length / 2;
        b.MaxX = f.Parameters.Length / 2;
        b.MinY = -f.Parameters.Width / 2;
        b.MaxY = f.Parameters.Width / 2;
    } else if (f.Shape == "Circular") {
        b.MinX = b.MinY = -f.Parameters.Radius;
        b.MaxX = b.MaxY = f.Parameters.Radius;
    } else if (f.Shape == "Polygon" && !f.Parameters.Corners.empty()) {
        b.MinX = b.MaxX = f.Parameters.Corners.front()[0];
        b.MinY = b.MaxY = f.Parameters.Corners.front()[1];
        for (const auto &p : f.Parameters.Corners) {
            b.MinX = std::min(b.MinX, p[0]);
            b.MaxX = std::max(b.MaxX, p[0]);
            b.MinY = std::min(b.MinY, p[1]);
            b.MaxY = std::max(b.MaxY, p[1]);
        }
    }
    m.DataInfo.Frequency =
        m.DataInfo.DT > 0 ? std::round(1 / m.DataInfo.DT) : 0;
}
DatasetDraft DatasetDraft::from_file(const std::filesystem::path &path) {
    auto f = read_qrest_file(utf8(path));
    require_report(validate_qrest_file(utf8(path)));
    DatasetDraft d;
    d.metadata_ = std::move(f.metadata);
    d.values_ = std::make_shared<const std::vector<double>>(
        std::move(f.channel_sequential_data));
    d.encoding_ = f.packet.data_encoding;
    d.source_id_ = f.packet.source_id;
    d.source_.path = utf8(std::filesystem::absolute(path));
    d.source_.description = "Original stored samples and channel Scale";
    check_values(d.metadata_, d.values());
    return d;
}
DatasetDraft
DatasetDraft::from_values(Metadata m,
                          std::shared_ptr<const std::vector<double>> v,
                          DraftSource s) {
    require(bool(v), "Missing owned input samples");
    if (s.normalized_physical) {
        for (auto &c : m.InstrumentInfo.Channels)
            c.Scale = 1;
        m.Extension["EditorSource"] = {
            {"path", s.path},
            {"dataset_id", s.dataset_id},
            {"snapshot_id", s.snapshot_id},
            {"representation", "physical values stored with Scale=1"}};
    }
    normalize_draft_metadata(m);
    check_values(m, *v);
    DatasetDraft d;
    d.metadata_ = std::move(m);
    d.values_ = std::move(v);
    d.source_ = std::move(s);
    return d;
}
void DatasetDraft::replace_metadata(Metadata m) {
    if (!values().empty()) {
        require(m.InstrumentInfo.Channels.size()
                    == metadata_.InstrumentInfo.Channels.size(),
                "Use channel transactions to change columns");
        require(m.DataInfo.NPTS == metadata_.DataInfo.NPTS,
                "NPTS is determined by imported samples");
    }
    normalize_draft_metadata(m);
    // Draft allows incomplete fields; invalid values remain visible for
    // correction.
    metadata_ = std::move(m);
    dirty_ = true;
}
void DatasetDraft::apply_json(const std::string &json) {
    auto m = Metadata::from_bytes(json);
    if (!values().empty())
        require(nlohmann::json(m.InstrumentInfo.Channels)
                    == nlohmann::json(metadata_.InstrumentInfo.Channels),
                "With waveforms present, edit channels using the channel page "
                "(JSON cannot reorder or replace columns)");
    normalize_draft_metadata(m);
    require_report(validate_metadata(m, {ValidationMode::Draft}));
    replace_metadata(std::move(m));
}
void DatasetDraft::edit_channel(std::size_t c, Channel channel) {
    auto m = metadata_;
    m.InstrumentInfo.Channels.at(c) = std::move(channel);
    replace_metadata(std::move(m));
}
void DatasetDraft::add_channel(Channel channel, std::vector<double> samples) {
    auto m = metadata_;
    auto v = std::make_shared<std::vector<double>>(values());
    if (!values().empty())
        require(samples.size() == std::size_t(m.DataInfo.NPTS),
                "New channel requires an explicit complete waveform; no "
                "implicit zeros");
    else
        require(samples.empty(), "Use import to establish sample dimensions");
    m.InstrumentInfo.Channels.push_back(std::move(channel));
    normalize_draft_metadata(m);
    v->insert(v->end(), samples.begin(), samples.end());
    metadata_ = std::move(m);
    values_ = std::move(v);
    dirty_ = true;
}
void DatasetDraft::duplicate_channel(std::size_t c) {
    auto channel = metadata_.InstrumentInfo.Channels.at(c);
    channel.ChannelID = "UNKNOWN";
    std::vector<double> samples;
    if (!values().empty()) {
        const auto n = std::size_t(metadata_.DataInfo.NPTS);
        samples.assign(values().begin() + c * n,
                       values().begin() + (c + 1) * n);
    }
    add_channel(std::move(channel), std::move(samples));
}
void DatasetDraft::select_channels(const std::vector<std::size_t> &order) {
    auto m = metadata_;
    m.InstrumentInfo.Channels.clear();
    auto v = std::make_shared<std::vector<double>>();
    std::set<std::size_t> seen;
    const auto n = std::size_t(std::max(0, metadata_.DataInfo.NPTS));
    for (auto c : order) {
        require(seen.insert(c).second,
                "Duplicate column in channel transaction");
        m.InstrumentInfo.Channels.push_back(
            metadata_.InstrumentInfo.Channels.at(c));
        if (!values().empty())
            v->insert(v->end(),
                      values().begin() + c * n,
                      values().begin() + (c + 1) * n);
    }
    normalize_draft_metadata(m);
    metadata_ = std::move(m);
    values_ = std::move(v);
    dirty_ = true;
}
void DatasetDraft::import_data(const ExternalDataset &data,
                               const std::vector<std::size_t> &order,
                               ImportAction action,
                               Metadata m,
                               bool physical) {
    require(data.sample_count > 0
                && data.sample_count
                       <= std::size_t(std::numeric_limits<int>::max()),
            "Invalid imported NPTS");
    require(data.channel_count > 0
                && data.sample_count <= std::numeric_limits<std::size_t>::max()
                                            / data.channel_count,
            "Invalid imported dimensions");
    require(data.channel_sequential_data.size()
                == data.sample_count * data.channel_count,
            "Imported matrix size mismatch");
    require(std::isfinite(data.sample_rate_hz) && data.sample_rate_hz > 0,
            "Explicit sample rate required");
    const std::size_t first = action == ImportAction::Append
                                  ? metadata_.InstrumentInfo.Channels.size()
                                  : 0;
    require(m.InstrumentInfo.Channels.size() == first + order.size()
                && !order.empty(),
            "Mapping must cover every destination channel");
    require(m.DataInfo.NPTS == int(data.sample_count)
                && close(1 / m.DataInfo.DT, data.sample_rate_hz),
            "Import NPTS/sample rate conflict");
    if (!data.distance_unit.empty())
        require(data.distance_unit == m.Units[0],
                "Import declared unit conflicts with destination unit");
    const auto time = parse_iso8601_timestamp_ms(m.DataInfo.StartTime);
    if (data.start_time_ms)
        require(time == *data.start_time_ms,
                "Import StartTime conflict; no automatic alignment");
    auto v = std::make_shared<std::vector<double>>();
    if (action == ImportAction::Append && !values().empty()) {
        require(m.DataInfo.NPTS == metadata_.DataInfo.NPTS
                    && close(m.DataInfo.DT, metadata_.DataInfo.DT)
                    && time
                           == parse_iso8601_timestamp_ms(
                               metadata_.DataInfo.StartTime)
                    && m.Units == metadata_.Units,
                "Append sample count/rate/time/unit conflict");
        for (std::size_t c = 0; c < first; ++c)
            require(nlohmann::json(m.InstrumentInfo.Channels[c])
                        == nlohmann::json(metadata_.InstrumentInfo.Channels[c]),
                    "Append cannot alter existing channels");
        *v = values();
    } else
        require(
            action != ImportAction::Append || first == 0,
            "Existing metadata channels have no waveforms; replace/fill first");
    std::set<std::size_t> seen;
    for (std::size_t c = 0; c < order.size(); ++c) {
        auto s = order[c];
        require(s < data.channel_count && seen.insert(s).second,
                "Invalid/duplicate source channel mapping");
        if (data.channel_measurands.size() == data.channel_count)
            require(
                m.InstrumentInfo.Channels[first + c].Measurand
                    == data.channel_measurands[s],
                "Import declared Measurand conflicts with destination channel");
        auto begin =
            data.channel_sequential_data.begin() + s * data.sample_count;
        v->insert(v->end(), begin, begin + data.sample_count);
        if (physical)
            m.InstrumentInfo.Channels[first + c].Scale = 1;
    }
    normalize_draft_metadata(m);
    check_values(m, *v);
    require_report(validate_metadata(m, {ValidationMode::Draft}));
    metadata_ = std::move(m);
    values_ = std::move(v);
    encoding_ = 1;
    dirty_ = true;
}
ValidationReport DatasetDraft::validate(ValidationMode mode) const {
    auto r = validate_metadata(metadata_, {mode});
    const auto issue = [&](std::string s) {
        (mode == ValidationMode::Final ? r.errors : r.warnings)
            .push_back(std::move(s));
    };
    if (metadata_.InstrumentInfo.Channels.size()
        > std::numeric_limits<std::uint16_t>::max())
        r.errors.push_back("Too many channels for qREST packet");
    if (values().size()
        > (std::numeric_limits<std::uint32_t>::max() - 32) / sizeof(double))
        r.errors.push_back("Packet exceeds qREST size limit");
    if (values().empty()
        || values().size()
               != std::size_t(std::max(0, metadata_.DataInfo.NPTS))
                      * metadata_.InstrumentInfo.Channels.size())
        issue("Data: missing waveform or matrix dimensions do not match "
              "metadata");
    for (std::size_t c = 0; c < metadata_.InstrumentInfo.Channels.size(); ++c) {
        const auto scale = metadata_.InstrumentInfo.Channels[c].Scale;
        const auto n = std::size_t(std::max(0, metadata_.DataInfo.NPTS));
        for (std::size_t i = c * n; i < std::min(values().size(), (c + 1) * n);
             ++i) {
            const auto x = values()[i];
            bool valid = std::isfinite(x) && std::isfinite(x * scale);
            if (encoding_ == 0)
                valid &= std::isfinite(static_cast<float>(x))
                         && double(static_cast<float>(x)) == x;
            if (encoding_ == 10)
                valid &= x >= -32768 && x <= 32767 && std::trunc(x) == x;
            if (encoding_ == 11)
                valid &=
                    x >= -2147483648. && x <= 2147483647. && std::trunc(x) == x;
            if (!valid) {
                r.errors.push_back(
                    "Data/channel " + std::to_string(c + 1)
                    + ": non-finite physical value or loss in packet encoding");
                break;
            }
        }
    }
    if (metadata_.BuildingInfo.StructuralFootprint.Shape.empty())
        issue("BuildingInfo.StructuralFootprint.Shape must be supplied");
    try {
        auto canonical = Metadata::from_bytes(metadata_.to_bytes());
        (void)canonical;
    } catch (const std::exception &e) {
        issue(std::string("Metadata serialization: ") + e.what());
    }
    return r;
}
void DatasetDraft::save_new(
    const std::filesystem::path &destination,
    const std::function<void(const std::filesystem::path &)> &verify) const {
    namespace fs = std::filesystem;
    require(destination.extension() == ".qrest",
            "Destination must be a new .qrest file");
    require_report(validate());
    auto target = fs::absolute(destination);
    auto parent = target.parent_path();
    require(!fs::exists(fs::symlink_status(target)),
            "Destination already exists (including symbolic links)");
    TempDirectory temp;
    static std::atomic<unsigned long long> serial{};
    for (int attempt = 0; attempt < 100; ++attempt) {
        auto name =
            ".qrest-editor-"
            + std::to_string(
                std::chrono::steady_clock::now().time_since_epoch().count())
            + "-" + std::to_string(++serial);
        auto p = parent / name;
        if (fs::create_directory(p)) {
            temp.path = std::move(p);
            break;
        }
    }
    require(!temp.path.empty(), "Cannot reserve temporary save directory");
    auto file = temp.path / "verified.qrest";
    write_qrest_file(utf8(file), metadata_, values(), source_id_, encoding_);
    auto read = read_qrest_file(utf8(file));
    require_report(validate_qrest_file(utf8(file)));
    require(read.metadata.to_bytes() == metadata_.to_bytes()
                && read.channel_sequential_data == values(),
            "Saved file round-trip differs from draft");
    require(read.packet.data_encoding == encoding_
                && read.packet.source_id == source_id_,
            "Saved packet encoding/source differs");
    if (verify)
        verify(file);
    // Atomic create-if-absent, including symlinks or a concurrently created
    // file. It never truncates the destination and never removes a published
    // file.
    fs::create_hard_link(file, target);
}
} // namespace qrest_data::tools
