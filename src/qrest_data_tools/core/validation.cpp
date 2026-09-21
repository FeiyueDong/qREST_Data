#include "validation.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <fstream>
#include <iterator>
#include <ostream>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>

#include "text_matrix.hpp"

namespace qrest_data::tools {
namespace {

std::string read_text_file(const std::string &path) {
    std::ifstream input(path, std::ios::binary);
    if (!input.is_open()) {
        throw std::runtime_error("Cannot open file: " + path);
    }
    return std::string((std::istreambuf_iterator<char>(input)),
                       std::istreambuf_iterator<char>());
}

void append_report(ValidationReport &dst, ValidationReport src) {
    dst.errors.insert(dst.errors.end(),
                      std::make_move_iterator(src.errors.begin()),
                      std::make_move_iterator(src.errors.end()));
    dst.warnings.insert(dst.warnings.end(),
                        std::make_move_iterator(src.warnings.begin()),
                        std::make_move_iterator(src.warnings.end()));
}

void add_error(ValidationReport &report, const std::string &message) {
    report.errors.push_back(message);
}

void add_warning(ValidationReport &report, const std::string &message) {
    report.warnings.push_back(message);
}

void add_issue(ValidationReport &report,
               ValidationMode mode,
               const std::string &message) {
    if (mode == ValidationMode::Final) {
        add_error(report, message);
    } else {
        add_warning(report, std::string("Incomplete draft: ") + message);
    }
}

std::string indexed_field(const char *prefix, std::size_t index) {
    std::ostringstream oss;
    oss << prefix << "[" << index << "]";
    return oss.str();
}

bool is_unknown_channel_id(const std::string &channel_id) {
    return channel_id == "UNKNOWN";
}

bool is_valid_azimuth(double azimuth) {
    constexpr double eps = 1e-9;
    return std::isfinite(azimuth)
           && (std::abs(azimuth + 1.0) < eps
               || (azimuth >= 0.0 && azimuth < 360.0));
}

std::uint64_t timestamp_or_error(ValidationReport &report,
                                 const std::string &start_time,
                                 ValidationMode mode) {
    if (start_time.empty()) {
        add_issue(report, mode, "DataInfo.StartTime must not be empty");
        return 0;
    }
    try {
        return parse_iso8601_timestamp_ms(start_time);
    } catch (const std::exception &e) {
        add_error(report,
                  std::string("DataInfo.StartTime is invalid: ") + e.what());
        return 0;
    }
}

std::uint16_t sampling_rate_or_error(ValidationReport &report,
                                     double dt,
                                     ValidationMode mode) {
    if (!(dt > 0.0) || !std::isfinite(dt)) {
        add_issue(report, mode, "DataInfo.DT must be positive and finite");
        return 0;
    }
    try {
        return sampling_rate_from_dt(dt);
    } catch (const std::exception &e) {
        add_error(report, std::string("DataInfo.DT is invalid: ") + e.what());
        return 0;
    }
}

} // namespace

ValidationReport validate_metadata(const Metadata &metadata,
                                   ValidationOptions options) {
    ValidationReport report;
    const ValidationMode mode = options.mode;

    if (metadata.Header != qrest_data::format::metadata_header) {
        add_error(report, "Header must be qREST_DATA");
    }
    if (metadata.Version[0] != qrest_data::format::metadata_major_version) {
        add_warning(report, "Metadata major version is not 1");
    }
    if (metadata.Units[0].empty() || metadata.Units[1].empty()) {
        add_error(report,
                  "Units must contain non-empty distance and time units");
    }
    if (metadata.Units[1] != "s") {
        add_error(report, "Units time unit must be s");
    }

    const auto &building = metadata.BuildingInfo;
    if (building.ElevationNum < 0) {
        add_error(report, "BuildingInfo.ElevationNum must not be negative");
    }
    if (building.Elevation.size()
        != static_cast<std::size_t>(building.ElevationNum)) {
        std::ostringstream oss;
        oss << "BuildingInfo.ElevationNum (" << building.ElevationNum
            << ") does not match Elevation size (" << building.Elevation.size()
            << ")";
        add_error(report, oss.str());
    }
    if (building.StructuralFootprint.Shape.empty()) {
        add_warning(report, "BuildingInfo.StructuralFootprint.Shape is empty");
    }
    const auto &footprint = building.StructuralFootprint;
    if (footprint.Shape == "Rectangular") {
        if (footprint.Parameters.Length <= 0.0
            || footprint.Parameters.Width <= 0.0
            || !std::isfinite(footprint.Parameters.Length)
            || !std::isfinite(footprint.Parameters.Width)) {
            add_error(report,
                      "BuildingInfo.StructuralFootprint rectangular "
                      "Length and Width must be positive and finite");
        }
    } else if (footprint.Shape == "Circular") {
        if (footprint.Parameters.Radius <= 0.0
            || !std::isfinite(footprint.Parameters.Radius)) {
            add_error(report,
                      "BuildingInfo.StructuralFootprint circular Radius "
                      "must be positive and finite");
        }
    } else if (footprint.Shape == "Polygon") {
        if (footprint.Parameters.Corners.size() < 3) {
            add_error(report,
                      "BuildingInfo.StructuralFootprint polygon requires at "
                      "least 3 corners");
        }
        for (const auto &corner : footprint.Parameters.Corners) {
            if (!std::isfinite(corner[0]) || !std::isfinite(corner[1])) {
                add_error(report,
                          "BuildingInfo.StructuralFootprint polygon corners "
                          "must be finite");
                break;
            }
        }
    } else if (!footprint.Shape.empty()) {
        add_error(report,
                  "BuildingInfo.StructuralFootprint.Shape is unsupported: "
                      + footprint.Shape);
    }
    for (double elevation : building.Elevation) {
        if (!std::isfinite(elevation)) {
            add_error(report, "BuildingInfo.Elevation values must be finite");
            break;
        }
    }
    for (std::size_t i = 1; i < building.Elevation.size(); ++i) {
        if (std::abs(building.Elevation[i] - building.Elevation[i - 1])
            < 1e-12) {
            add_error(report,
                      "BuildingInfo.Elevation contains duplicate value");
            break;
        }
        if (building.Elevation[i] <= building.Elevation[i - 1]) {
            add_error(report,
                      "BuildingInfo.Elevation must be strictly increasing");
            break;
        }
    }

    const auto &instrument = metadata.InstrumentInfo;
    if (instrument.ChannelNum <= 0) {
        add_issue(report, mode, "InstrumentInfo.ChannelNum must be positive");
    }
    if (instrument.Channels.size()
        != static_cast<std::size_t>(std::max(instrument.ChannelNum, 0))) {
        std::ostringstream oss;
        oss << "InstrumentInfo.ChannelNum (" << instrument.ChannelNum
            << ") does not match Channels size (" << instrument.Channels.size()
            << ")";
        add_error(report, oss.str());
    }
    std::set<std::string> channel_ids;
    int unknown_channel_id_count = 0;
    for (std::size_t i = 0; i < instrument.Channels.size(); ++i) {
        const auto &channel = instrument.Channels[i];
        const int expected_no = static_cast<int>(i + 1);
        const std::string field = indexed_field("InstrumentInfo.Channels", i);
        if (channel.ChannelNo != expected_no) {
            std::ostringstream oss;
            oss << field << ".ChannelNo must be " << expected_no << ", got "
                << channel.ChannelNo;
            add_error(report, oss.str());
        }
        if (channel.ChannelID.empty()) {
            add_warning(report, field + ".ChannelID is empty");
        } else if (is_unknown_channel_id(channel.ChannelID)) {
            ++unknown_channel_id_count;
        } else if (!channel_ids.insert(channel.ChannelID).second) {
            add_error(report,
                      field + ".ChannelID is duplicated: " + channel.ChannelID);
        }
        if (channel.DeviceType.empty()) {
            add_issue(report, mode, field + ".DeviceType must not be empty");
        }
        if (channel.Measurand.empty()) {
            add_error(report, field + ".Measurand must not be empty");
        }
        if (!std::isfinite(channel.Scale) || channel.Scale == 0.0) {
            add_error(report, field + ".Scale must be finite and non-zero");
        }
        if (!is_valid_azimuth(channel.Azimuth)) {
            add_error(report, field + ".Azimuth must be -1 or within [0, 360)");
        }
        for (double coord : channel.LocationXYZ) {
            if (!std::isfinite(coord)) {
                add_error(report, field + ".LocationXYZ must be finite");
                break;
            }
        }
    }
    if (unknown_channel_id_count > 0) {
        std::ostringstream oss;
        oss << unknown_channel_id_count << " channels use UNKNOWN ChannelID";
        add_warning(report, oss.str());
    }

    const auto &data = metadata.DataInfo;
    if (data.NPTS <= 0) {
        add_issue(report, mode, "DataInfo.NPTS must be positive");
    }
    sampling_rate_or_error(report, data.DT, mode);
    timestamp_or_error(report, data.StartTime, mode);
    if (data.Corrected.empty()) {
        add_warning(report, "DataInfo.Corrected is empty");
    }

    return report;
}

ValidationReport
validate_qrest_content(const Metadata &metadata,
                       std::uint16_t packet_channel_count,
                       std::uint16_t packet_sampling_rate,
                       std::uint32_t packet_data_point_count,
                       std::uint64_t packet_timestamp_ms,
                       std::size_t channel_sequential_value_count,
                       ValidationOptions options) {
    ValidationReport report = validate_metadata(metadata, options);
    const ValidationMode mode = options.mode;

    const auto &channels = metadata.InstrumentInfo.Channels;
    if (packet_channel_count != static_cast<std::uint16_t>(channels.size())) {
        std::ostringstream oss;
        oss << "Packet channel_count (" << packet_channel_count
            << ") does not match Channels size (" << channels.size() << ")";
        add_issue(report, mode, oss.str());
    }
    if (metadata.DataInfo.NPTS >= 0
        && packet_data_point_count
               != static_cast<std::uint32_t>(metadata.DataInfo.NPTS)) {
        std::ostringstream oss;
        oss << "Packet data_point_count (" << packet_data_point_count
            << ") does not match DataInfo.NPTS (" << metadata.DataInfo.NPTS
            << ")";
        add_issue(report, mode, oss.str());
    }

    if (metadata.DataInfo.DT > 0.0 && std::isfinite(metadata.DataInfo.DT)) {
        const std::uint16_t metadata_sampling_rate =
            sampling_rate_or_error(report, metadata.DataInfo.DT, mode);
        if (metadata_sampling_rate != 0
            && packet_sampling_rate != metadata_sampling_rate) {
            std::ostringstream oss;
            oss << "Packet sampling_rate (" << packet_sampling_rate
                << ") does not match DataInfo.DT-derived sampling rate ("
                << metadata_sampling_rate << ")";
            add_issue(report, mode, oss.str());
        }
    }

    if (!metadata.DataInfo.StartTime.empty()) {
        const std::uint64_t metadata_timestamp =
            timestamp_or_error(report, metadata.DataInfo.StartTime, mode);
        if (metadata_timestamp != 0
            && packet_timestamp_ms != metadata_timestamp) {
            std::ostringstream oss;
            oss << "Packet timestamp_ms (" << packet_timestamp_ms
                << ") does not match DataInfo.StartTime (" << metadata_timestamp
                << ")";
            add_issue(report, mode, oss.str());
        }
    }

    const std::size_t expected_values =
        static_cast<std::size_t>(packet_channel_count)
        * packet_data_point_count;
    if (channel_sequential_value_count != expected_values) {
        std::ostringstream oss;
        oss << "Packet data size mismatch: expected " << expected_values
            << ", got " << channel_sequential_value_count;
        add_issue(report, mode, oss.str());
    }

    return report;
}

ValidationReport validate_qrest_file(const std::string &path) {
    ValidationReport report;
    QrestFileData file;
    try {
        file = read_qrest_file(path);
    } catch (const std::exception &e) {
        add_error(report, e.what());
        return report;
    }

    return validate_qrest_content(file.metadata,
                                  file.packet.channel_count,
                                  file.packet.sampling_rate,
                                  file.packet.data_point_count,
                                  file.packet.timestamp_ms,
                                  file.channel_sequential_data.size());
}

ValidationReport validate_text_dataset(const std::string &metadata_path,
                                       const std::string &data_path) {
    ValidationReport report;
    Metadata metadata;
    try {
        metadata = Metadata::from_bytes(read_text_file(metadata_path));
    } catch (const std::exception &e) {
        add_error(report, std::string("Metadata parse failed: ") + e.what());
        return report;
    }

    append_report(report, validate_metadata(metadata));
    if (!report.ok()) {
        return report;
    }

    try {
        const auto data = read_time_major_text_matrix(
            data_path,
            static_cast<std::size_t>(metadata.InstrumentInfo.ChannelNum),
            static_cast<std::size_t>(metadata.DataInfo.NPTS));
        const std::size_t expected_values =
            static_cast<std::size_t>(metadata.InstrumentInfo.ChannelNum)
            * metadata.DataInfo.NPTS;
        if (data.size() != expected_values) {
            std::ostringstream oss;
            oss << "Text matrix value count mismatch: expected "
                << expected_values << ", got " << data.size();
            add_error(report, oss.str());
        }
    } catch (const std::exception &e) {
        add_error(report, e.what());
    }

    return report;
}

void print_validation_report(std::ostream &out,
                             const std::string &subject,
                             const ValidationReport &report) {
    out << (report.ok() ? "[OK] " : "[FAILED] ") << subject << '\n';
    for (const auto &warning : report.warnings) {
        out << "  warning: " << warning << '\n';
    }
    for (const auto &error : report.errors) {
        out << "  error: " << error << '\n';
    }
}

} // namespace qrest_data::tools
