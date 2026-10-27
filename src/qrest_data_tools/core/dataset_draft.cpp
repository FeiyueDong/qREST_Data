/**
**            qREST - Quick Response Evaluation for Safety Tagging
**     Institute of Engineering Mechanics, China Earthquake Administration
**                 Copyright 2024 - 2026 QLab, Dong Feiyue
**                          All Rights Reserved.
** Project: qREST
** File: /src/qrest_data_tools/core/dataset_draft.cpp
** File Created: Sunday, 18 October 2026
** Author: Codex
** Last Modified: Tuesday, 27 October 2026
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
    DraftSource source;
    source.path = utf8(std::filesystem::absolute(path));
    source.description = "Original stored samples and channel Scale";
    auto d = from_values(std::move(f.metadata),
                         std::make_shared<const std::vector<double>>(
                             std::move(f.channel_sequential_data)),
                         source);
    d.encoding_ = f.packet.data_encoding;
    d.source_id_ = f.packet.source_id;
    return d;
}
DatasetDraft
DatasetDraft::from_values(Metadata m,
                          std::shared_ptr<const std::vector<double>> v,
                          DraftSource source) {
    require(bool(v), "Missing owned input samples");
    normalize_draft_metadata(m);
    check_values(m, *v);
    if (source.normalized_physical) {
        for (auto &c : m.InstrumentInfo.Channels)
            c.Scale = 1;
        m.Extension["EditorSource"] = {
            {"path", source.path},
            {"dataset_id", source.dataset_id},
            {"snapshot_id", source.snapshot_id},
            {"representation", "physical values stored with Scale=1"}};
    }
    DatasetDraft d;
    d.metadata_ = std::move(m);
    d.values_ = std::move(v);
    d.source_ = std::move(source);
    d.sample_count_ = d.metadata_.DataInfo.NPTS;
    d.sampling_rate_ = 1 / d.metadata_.DataInfo.DT;
    require(std::isfinite(d.sampling_rate_) && d.sampling_rate_ > 0,
            "Invalid input sample rate");
    d.start_time_ = parse_iso8601_timestamp_ms(d.metadata_.DataInfo.StartTime);
    for (const auto &c : d.metadata_.InstrumentInfo.Channels) {
        d.metadata_uids_.push_back(d.next_uid_++);
        d.columns_.push_back({d.next_uid_++,
                              {},
                              d.source_.path,
                              c.Measurand,
                              d.metadata_.Units[0],
                              d.source_.normalized_physical,
                              c.Scale});
    }
    d.initial_binding();
    return d;
}
std::optional<std::size_t> DatasetDraft::metadata_index(Uid uid) const {
    auto it = std::find(metadata_uids_.begin(), metadata_uids_.end(), uid);
    if (it == metadata_uids_.end())
        return {};
    return std::size_t(it - metadata_uids_.begin());
}
std::size_t DatasetDraft::column_index(Uid uid) const {
    for (std::size_t i = 0; i < columns_.size(); ++i)
        if (columns_[i].uid == uid)
            return i;
    throw std::invalid_argument("Unknown data column UID");
}
std::optional<std::size_t> DatasetDraft::column_for(Uid uid) const {
    for (std::size_t i = 0; i < columns_.size(); ++i)
        if (columns_[i].bound_channel == uid)
            return i;
    return {};
}
std::optional<DatasetDraft::Uid> DatasetDraft::binding(Uid uid) const {
    return columns_.at(column_index(uid)).bound_channel;
}
std::vector<DatasetDraft::Uid>
DatasetDraft::available_channels(Uid current) const {
    auto index = column_index(current);
    std::vector<Uid> out;
    for (auto uid : metadata_uids_) {
        auto occupied = column_for(uid);
        if (!occupied || *occupied == index)
            out.push_back(uid);
    }
    return out;
}
void DatasetDraft::bind_column(Uid column, Uid target) {
    auto index = column_index(column);
    require(metadata_index(target).has_value(), "Unknown Metadata channel UID");
    auto occupied = column_for(target);
    require(!occupied || *occupied == index,
            "Metadata channel is already bound; unbind or swap explicitly");
    columns_[index].bound_channel = target;
    dirty_ = true;
}
void DatasetDraft::unbind_column(Uid column) {
    columns_.at(column_index(column)).bound_channel.reset();
    dirty_ = true;
}
void DatasetDraft::swap_bindings(Uid a, Uid b) {
    auto ai = column_index(a), bi = column_index(b);
    require(ai != bi && columns_[ai].bound_channel
                && columns_[bi].bound_channel,
            "Swap requires two distinct bound real data columns");
    std::swap(columns_[ai].bound_channel, columns_[bi].bound_channel);
    dirty_ = true;
}
void DatasetDraft::take_over_binding(Uid column,
                                     Uid channel,
                                     Uid expected_owner) {
    const auto target = column_index(column);
    const auto previous = column_index(expected_owner);
    require(metadata_index(channel).has_value(),
            "Unknown Metadata channel UID");
    require(target != previous && !columns_[target].bound_channel,
            "Takeover requires a distinct unbound real data column");
    require(columns_[previous].bound_channel == channel
                && column_for(channel) == previous,
            "Metadata channel owner changed; confirm the new owner first");
    columns_[previous].bound_channel.reset();
    columns_[target].bound_channel = channel;
    dirty_ = true;
}
void DatasetDraft::unbind_all_columns() {
    for (auto &column : columns_) {
        if (column.bound_channel) {
            column.bound_channel.reset();
            dirty_ = true;
        }
    }
}
void DatasetDraft::bind_unassigned_sequentially() {
    std::vector<Uid> free;
    for (auto uid : metadata_uids_)
        if (!column_for(uid))
            free.push_back(uid);
    std::size_t i = 0;
    for (auto &col : columns_)
        if (!col.bound_channel && i < free.size())
            col.bound_channel = free[i++];
    if (i) {
        initially_bound_ = true;
        dirty_ = true;
    }
}
void DatasetDraft::initial_binding() {
    if (!initially_bound_ && !metadata_uids_.empty() && !columns_.empty()) {
        const auto dirty = dirty_;
        bind_unassigned_sequentially();
        dirty_ = dirty;
    }
}
std::size_t DatasetDraft::unbound_data_count() const {
    return std::count_if(columns_.begin(), columns_.end(), [](const auto &c) {
        return !c.bound_channel;
    });
}
void DatasetDraft::replace_metadata(Metadata m) {
    require(m.InstrumentInfo.Channels.size() == metadata_uids_.size(),
            "Use Metadata channel transactions or full Metadata import to "
            "change channels");
    normalize_draft_metadata(m);
    metadata_ = std::move(m);
    dirty_ = true;
}
void DatasetDraft::correct_start_time(const std::string &iso8601) {
    require(!columns_.empty(),
            "Time correction requires real waveform columns");
    const auto target = parse_iso8601_timestamp_ms(iso8601);
    require(target <= std::uint64_t(std::numeric_limits<std::int64_t>::max()),
            "Time correction target is out of range");
    // The source parser accepts additional fractional digits. Do not silently
    // truncate a requested sub-millisecond correction into the ms contract.
    const auto dot = iso8601.find('.');
    if (dot != std::string::npos) {
        std::size_t digits = 0;
        for (auto i = dot + 1;
             i < iso8601.size() && iso8601[i] >= '0' && iso8601[i] <= '9';
             ++i, ++digits)
            if (digits >= 3)
                require(iso8601[i] == '0',
                        "Time correction must be exactly representable in "
                        "milliseconds");
    }
    auto candidate = metadata_;
    candidate.DataInfo.StartTime = iso8601;
    if (start_time_ != target) {
        nlohmann::json original = start_time_ ? nlohmann::json(*start_time_)
                                              : nlohmann::json(nullptr);
        const auto old = candidate.Extension.find("EditorTimeCorrection");
        if (old != candidate.Extension.end()) {
            require(old->is_object() && old->value("schema_version", 0) == 1
                        && old->contains("original_start_time_ms")
                        && old->contains("effective_start_time_ms")
                        && start_time_
                        && (*old)["effective_start_time_ms"] == *start_time_,
                    "Existing EditorTimeCorrection provenance conflicts with "
                    "effective time");
            original = old->at("original_start_time_ms");
            require(original.is_null()
                        || (original.is_number_unsigned()
                            && original.get<std::uint64_t>() <= std::uint64_t(
                                   std::numeric_limits<std::int64_t>::max())),
                    "Invalid original time correction provenance");
        }
        candidate.Extension["EditorTimeCorrection"] = {
            {"schema_version", 1},
            {"original_start_time_ms", original},
            {"effective_start_time_ms", target},
            {"offset_ms",
             original.is_null()
                 ? nlohmann::json(nullptr)
                 : nlohmann::json(static_cast<std::int64_t>(target)
                                  - static_cast<std::int64_t>(
                                      original.get<std::uint64_t>()))}};
    }
    metadata_ = std::move(candidate);
    start_time_ = target;
    dirty_ = true;
}
void DatasetDraft::import_metadata(Metadata m) {
    normalize_draft_metadata(m);
    std::vector<Uid> ids;
    for (std::size_t i = 0; i < m.InstrumentInfo.Channels.size(); ++i)
        ids.push_back(next_uid_++);
    metadata_ = std::move(m);
    metadata_uids_ = std::move(ids);
    for (auto &c : columns_)
        c.bound_channel.reset();
    initial_binding();
    dirty_ = true;
}
void DatasetDraft::apply_json(const std::string &json) {
    const auto parsed = nlohmann::json::parse(json);
    const double dt = parsed.at("DataInfo").at("DT").get<double>();
    if (dt > 0)
        (void)sampling_rate_from_dt(dt);
    auto m = Metadata::from_bytes(json);
    import_metadata(std::move(m));
}
void DatasetDraft::edit_channel(std::size_t c, Channel channel) {
    metadata_.InstrumentInfo.Channels.at(c) = std::move(channel);
    normalize_draft_metadata(metadata_);
    dirty_ = true;
}
void DatasetDraft::add_channel(Channel channel) {
    metadata_.InstrumentInfo.Channels.push_back(std::move(channel));
    metadata_uids_.push_back(next_uid_++);
    normalize_draft_metadata(metadata_);
    initial_binding();
    dirty_ = true;
}
void DatasetDraft::duplicate_channel(std::size_t c) {
    auto copy = metadata_.InstrumentInfo.Channels.at(c);
    add_channel(std::move(copy));
}
void DatasetDraft::reorder_metadata_channels(const std::vector<Uid> &order) {
    std::set<Uid> seen;
    auto m = metadata_;
    m.InstrumentInfo.Channels.clear();
    for (auto uid : order) {
        auto index = metadata_index(uid);
        require(index && seen.insert(uid).second,
                "Invalid/duplicate Metadata UID");
        m.InstrumentInfo.Channels.push_back(
            metadata_.InstrumentInfo.Channels[*index]);
    }
    normalize_draft_metadata(m);
    metadata_ = std::move(m);
    metadata_uids_ = order;
    for (auto &c : columns_)
        if (c.bound_channel && !seen.contains(*c.bound_channel))
            c.bound_channel.reset();
    dirty_ = true;
}
void DatasetDraft::import_waveforms(const ExternalDataset &data,
                                    ImportAction action,
                                    bool physical,
                                    const std::vector<double> &source_scales,
                                    const std::string &source_label) {
    require(data.sample_count > 0
                && data.sample_count
                       <= std::size_t(std::numeric_limits<int>::max())
                && data.channel_count > 0
                && data.sample_count <= std::numeric_limits<std::size_t>::max()
                                            / data.channel_count,
            "Invalid imported dimensions");
    require(data.channel_sequential_data.size()
                == data.sample_count * data.channel_count,
            "Imported matrix size mismatch");
    require(std::isfinite(data.sample_rate_hz) && data.sample_rate_hz > 0,
            "Explicit valid source sample rate required");
    for (double x : data.channel_sequential_data)
        require(std::isfinite(x), "Source contains NaN/Inf");
    require(source_scales.empty() || source_scales.size() == data.channel_count,
            "Source Scale count mismatch");
    for (double scale : source_scales)
        require(std::isfinite(scale) && scale != 0, "Invalid source Scale");
    require(data.channel_measurands.empty()
                || data.channel_measurands.size() == data.channel_count,
            "Source quantity count mismatch");
    const bool append = action == ImportAction::Append && !columns_.empty();
    if (append) {
        require(sample_count_ == data.sample_count
                    && close(sampling_rate_, data.sample_rate_hz)
                    && start_time_ == data.start_time_ms,
                "Append source sample count/rate/start time conflict");
        for (const auto &c : columns_)
            require(c.distance_unit.empty() || data.distance_unit.empty()
                        || c.distance_unit == data.distance_unit,
                    "Append source unit conflict");
    }
    auto buffer = std::make_shared<std::vector<double>>();
    auto cols = append ? columns_ : std::vector<DataColumn>{};
    if (append)
        *buffer = values();
    buffer->insert(buffer->end(),
                   data.channel_sequential_data.begin(),
                   data.channel_sequential_data.end());
    auto next = next_uid_;
    for (std::size_t i = 0; i < data.channel_count; ++i)
        cols.push_back(
            {next++,
             {},
             (source_label.empty() ? data.source_format : source_label) + " D"
                 + std::to_string(i + 1),
             data.channel_measurands.empty() ? "" : data.channel_measurands[i],
             data.distance_unit,
             physical,
             source_scales.empty() ? std::optional<double>{}
                                   : source_scales[i]});
    auto metadata = metadata_;
    if (!append)
        metadata.Extension.erase("EditorTimeCorrection");
    values_ = std::move(buffer);
    columns_ = std::move(cols);
    metadata_ = std::move(metadata);
    next_uid_ = next;
    sample_count_ = data.sample_count;
    sampling_rate_ = data.sample_rate_hz;
    start_time_ = data.start_time_ms;
    if (!append)
        initially_bound_ = false;
    initial_binding();
    encoding_ = 1;
    dirty_ = true;
}
ValidationReport DatasetDraft::validate(ValidationMode mode) const {
    auto r = validate_metadata(metadata_, {mode});
    auto issue = [&](std::string s) {
        (mode == ValidationMode::Final ? r.errors : r.warnings)
            .push_back(std::move(s));
    };
    if (metadata_uids_.empty())
        issue("Metadata: no channels supplied");
    if (metadata_uids_.size() > std::numeric_limits<std::uint16_t>::max())
        r.errors.push_back("Too many channels for qREST packet");
    if (sample_count_
        && metadata_uids_.size()
               > (std::numeric_limits<std::uint32_t>::max() - 32)
                     / sizeof(double) / sample_count_)
        r.errors.push_back("Projected packet exceeds qREST size limit");
    if (!columns_.empty()) {
        if (sample_count_ != std::size_t(std::max(0, metadata_.DataInfo.NPTS)))
            issue("Dataset: source NPTS differs from Metadata");
        if (!(metadata_.DataInfo.DT > 0)
            || !close(1 / metadata_.DataInfo.DT, sampling_rate_))
            issue("Dataset: source sample rate differs from Metadata DT");
        try {
            if (start_time_
                && *start_time_
                       != parse_iso8601_timestamp_ms(
                           metadata_.DataInfo.StartTime))
                issue("Dataset: source StartTime differs from Metadata");
        } catch (const std::exception &e) {
            issue(std::string("Dataset: ") + e.what());
        }
    }
    for (std::size_t c = 0; c < metadata_uids_.size(); ++c) {
        auto col = column_for(metadata_uids_[c]);
        auto prefix = "Channel[" + std::to_string(c) + "]: ";
        if (!col) {
            issue(prefix + "no real waveform bound");
            continue;
        }
        const auto &fact = columns_[*col];
        const auto &target = metadata_.InstrumentInfo.Channels[c];
        if (!fact.distance_unit.empty()
            && fact.distance_unit != metadata_.Units[0])
            issue(prefix + "source unit differs from Metadata unit");
        if (!fact.measurand.empty() && fact.measurand != target.Measurand)
            issue(prefix + "source quantity differs from Metadata Measurand");
        if (!fact.physical && fact.source_scale
            && target.Scale != *fact.source_scale)
            issue(prefix + "source stored Scale differs from Metadata Scale");
        if (fact.physical && target.Scale != 1)
            issue(prefix + "physical source values require explicit Scale=1");
        for (std::size_t t = 0; t < sample_count_; ++t) {
            auto x = values()[*col * sample_count_ + t];
            bool valid = std::isfinite(x) && std::isfinite(x * target.Scale);
            if (encoding_ == 0)
                valid &= std::isfinite(static_cast<float>(x))
                         && double(static_cast<float>(x)) == x;
            if (encoding_ == 10)
                valid &= x >= -32768 && x <= 32767 && std::trunc(x) == x;
            if (encoding_ == 11)
                valid &=
                    x >= -2147483648. && x <= 2147483647. && std::trunc(x) == x;
            if (!valid) {
                issue(prefix
                      + "non-finite physical value or loss in packet encoding");
                break;
            }
        }
    }
    auto omitted = unbound_data_count();
    if (omitted)
        r.warnings.push_back(
            "当前草稿包含 " + std::to_string(omitted)
            + " 条未绑定波形数据列。这些数据列不会写入正式 .qrest "
              "文件；已绑定的有效列将按 Metadata 通道编号输出。");
    if (metadata_.BuildingInfo.StructuralFootprint.Shape.empty())
        issue("BuildingInfo.StructuralFootprint.Shape must be supplied");
    return r;
}
DatasetDraft::FormalPayload DatasetDraft::project_formal_output() const {
    require_report(validate());
    FormalPayload p{metadata_, {}, unbound_data_count()};
    p.values.reserve(sample_count_ * metadata_uids_.size());
    for (auto uid : metadata_uids_) {
        auto c = *column_for(uid);
        auto begin = values().begin() + c * sample_count_;
        p.values.insert(p.values.end(), begin, begin + sample_count_);
    }
    return p;
}
void DatasetDraft::save_new(
    const std::filesystem::path &destination,
    const std::function<void(const std::filesystem::path &)> &verify) const {
    namespace fs = std::filesystem;
    require(destination.extension() == ".qrest",
            "Destination must be a new .qrest file");
    const auto output = project_formal_output();
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
    write_qrest_file(
        utf8(file), output.metadata, output.values, source_id_, encoding_);
    auto read = read_qrest_file(utf8(file));
    require_report(validate_qrest_file(utf8(file)));
    require(read.metadata.to_bytes() == output.metadata.to_bytes()
                && read.channel_sequential_data == output.values,
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
