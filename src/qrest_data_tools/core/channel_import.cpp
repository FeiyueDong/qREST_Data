/**
**            qREST - Quick Response Evaluation for Safety Tagging
**     Institute of Engineering Mechanics, China Earthquake Administration
**                 Copyright 2024 - 2026 QLab, Dong Feiyue
**                          All Rights Reserved.
** Project: qREST
** File: /src/qrest_data_tools/core/channel_import.cpp
** File Created: Wednesday, 28 October 2026
** Author: Codex
** Last Modified: Wednesday, 28 October 2026
** Modified By: Codex
*/
#include "channel_import.hpp"
#include <chrono>
#include <cmath>
#include <iomanip>
#include <limits>
#include <sstream>
#include <stdexcept>
#include "dataset_draft.hpp"

namespace qrest_data::tools {
std::string channel_import_start_time(const ExternalDataset &d) {
    if (!d.start_time_ms)
        return {};
    using namespace std::chrono;
    const sys_time<milliseconds> point{milliseconds{*d.start_time_ms}};
    const auto day = floor<days>(point);
    const year_month_day date{day};
    const hh_mm_ss clock{point - day};
    std::ostringstream out;
    out << std::setfill('0') << std::setw(4) << int(date.year()) << '-'
        << std::setw(2) << unsigned(date.month()) << '-' << std::setw(2)
        << unsigned(date.day()) << 'T' << std::setw(2) << clock.hours().count()
        << ':' << std::setw(2) << clock.minutes().count() << ':' << std::setw(2)
        << clock.seconds().count() << '.' << std::setw(3)
        << clock.subseconds().count() << 'Z';
    return out.str();
}
std::string channel_import_error(const ChannelImport &input) {
    const auto &d = input.data;
    if (!d.channel_count
        || d.channel_count > std::numeric_limits<std::uint16_t>::max()
        || !d.sample_count
        || d.sample_count > std::size_t(std::numeric_limits<int>::max())
        || d.sample_count
               > std::numeric_limits<std::size_t>::max() / d.channel_count
        || d.channel_sequential_data.size() != d.channel_count * d.sample_count
        || input.scales.size() != d.channel_count
        || input.scale_origins.size() != d.channel_count)
        return "Invalid channel matrix/Scale dimensions";
    if (!std::isfinite(d.sample_rate_hz) || d.sample_rate_hz < 0)
        return "Invalid source sample rate";
    for (const auto &quantity : d.channel_measurands)
        if (!quantity.empty() && quantity != "Acceleration"
            && quantity != "RawCounts" && quantity != "Unknown")
            return "Source quantity is not acceleration";
    if (d.start_time_us
        && (*d.start_time_us < 0 || *d.start_time_us % 1000 || !d.start_time_ms
            || *d.start_time_ms != std::uint64_t(*d.start_time_us / 1000)))
        return "Source UTC microseconds=" + std::to_string(*d.start_time_us)
               + " cannot be represented exactly by the draft millisecond time "
                 "axis";
    for (const auto scale : input.scales)
        if (!std::isfinite(scale) || scale == 0)
            return "Scale must be finite and non-zero";
    for (const auto value : d.channel_sequential_data)
        if (!std::isfinite(value))
            return "Source contains NaN/Inf";
    return {};
}
void DatasetDraft::import_channels(const ChannelImport &input,
                                   ImportAction action,
                                   const std::vector<double> &scales) {
    if (const auto error = channel_import_error(input); !error.empty())
        throw std::runtime_error(error);
    const auto &raw = input.data;
    if (scales.size() != raw.channel_count)
        throw std::runtime_error("Scale count differs from imported channels");
    if (action == ImportAction::Append)
        for (const auto &column : columns_)
            if (!column.measurand.empty() && column.measurand != "Acceleration")
                throw std::runtime_error("Append source quantity conflict: "
                                         + column.measurand);
    const auto unit = metadata_.Units[0].empty() ? "m" : metadata_.Units[0];
    const double meter_per_unit = unit == "m" ? 1.0 : unit == "cm" ? 0.01 : 0;
    if (!metadata_.Units[1].empty() && metadata_.Units[1] != "s")
        throw std::runtime_error("Channel import requires dataset time unit s");
    if (!meter_per_unit)
        throw std::runtime_error("Channel import requires dataset length unit "
                                 "m or cm; geometry is never relabeled");
    ExternalDataset data = raw;
    data.distance_unit = unit;
    data.output_unit = unit + "/s^2";
    data.representation = "Physical";
    data.channel_measurands.assign(data.channel_count, "Acceleration");
    for (std::size_t c = 0; c < data.channel_count; ++c) {
        if (!std::isfinite(scales[c]) || scales[c] == 0)
            throw std::runtime_error("Channel " + std::to_string(c + 1)
                                     + ": Scale must be finite and non-zero");
        for (std::size_t t = 0; t < data.sample_count; ++t) {
            auto &value =
                data.channel_sequential_data[c * data.sample_count + t];
            value = (value * scales[c]) / meter_per_unit;
            if (!std::isfinite(value))
                throw std::runtime_error("Channel " + std::to_string(c + 1)
                                         + ": Scale produces NaN/Inf");
        }
    }
    // All state changes, including sampling adoption and audit, are atomic.
    auto candidate = *this;
    const bool append = action == ImportAction::Append && !columns_.empty();
    const auto first = append ? columns_.size() : 0;
    candidate.import_waveforms(data, action, true, {}, input.source);
    auto m = candidate.metadata_;
    m.Units[0] = unit;
    m.Units[1] = "s";
    m.DataInfo.NPTS = static_cast<int>(data.sample_count);
    if (data.sample_rate_hz > 0 && (!append || !(m.DataInfo.DT > 0)))
        m.DataInfo.DT = 1 / data.sample_rate_hz;
    if (data.start_time_ms && (!append || m.DataInfo.StartTime.empty()))
        m.DataInfo.StartTime = channel_import_start_time(data);
    nlohmann::json imported = nlohmann::json::array();
    for (std::size_t c = 0; c < data.channel_count; ++c) {
        const auto &column = candidate.columns_[first + c];
        if (column.bound_channel) {
            auto &channel =
                m.InstrumentInfo
                    .Channels[*candidate.metadata_index(*column.bound_channel)];
            channel.Scale = 1;
            channel.Measurand = "Acceleration";
        }
        imported.push_back(
            {{"label",
              c < raw.channel_labels.size() ? raw.channel_labels[c]
                                            : "D" + std::to_string(c + 1)},
             {"scale_to_mps2", scales[c]},
             {"default_scale_to_mps2", input.scales[c]},
             {"scale_origin",
              scales[c] == input.scales[c] ? input.scale_origins[c] : "user"}});
    }
    auto &audit = m.Extension["ChannelImports"];
    if (audit.is_null() || !append)
        audit = nlohmann::json::array();
    if (!audit.is_array())
        throw std::runtime_error("ChannelImports extension must be an array");
    audit.push_back({{"schema_version", 1},
                     {"source", input.source},
                     {"format", input.format},
                     {"transpose_text", input.transpose_text},
                     {"columns", std::move(imported)},
                     {"sample_count", data.sample_count},
                     {"source_sample_rate_hz", data.sample_rate_hz},
                     {"source_start_time", channel_import_start_time(raw)},
                     {"stored_unit", data.output_unit}});
    candidate.replace_metadata(std::move(m));
    *this = std::move(candidate);
}
} // namespace qrest_data::tools
