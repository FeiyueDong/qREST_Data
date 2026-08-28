#include "validation.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <fstream>
#include <iterator>
#include <ostream>
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

std::string indexed_field(const char *prefix, std::size_t index) {
    std::ostringstream oss;
    oss << prefix << "[" << index << "]";
    return oss.str();
}

std::uint64_t timestamp_or_error(ValidationReport &report,
                                 const std::string &start_time) {
    try {
        return parse_iso8601_timestamp_ms(start_time);
    } catch (const std::exception &e) {
        add_error(report,
                  std::string("DataInfo.StartTime is invalid: ") + e.what());
        return 0;
    }
}

std::uint16_t sampling_rate_or_error(ValidationReport &report, double dt) {
    try {
        return sampling_rate_from_dt(dt);
    } catch (const std::exception &e) {
        add_error(report, std::string("DataInfo.DT is invalid: ") + e.what());
        return 0;
    }
}

} // namespace

ValidationReport validate_metadata(const Metadata &metadata) {
    ValidationReport report;

    if (metadata.Header != "qREST_DATA") {
        add_error(report, "Header must be qREST_DATA");
    }
    if (metadata.Version[0] != 1) {
        add_warning(report, "Metadata major version is not 1");
    }
    if (metadata.Units[0].empty() || metadata.Units[1].empty()) {
        add_error(report,
                  "Units must contain non-empty distance and time units");
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

    const auto &instrument = metadata.InstrumentInfo;
    if (instrument.ChannelNum <= 0) {
        add_error(report, "InstrumentInfo.ChannelNum must be positive");
    }
    if (instrument.Channels.size()
        != static_cast<std::size_t>(std::max(instrument.ChannelNum, 0))) {
        std::ostringstream oss;
        oss << "InstrumentInfo.ChannelNum (" << instrument.ChannelNum
            << ") does not match Channels size (" << instrument.Channels.size()
            << ")";
        add_error(report, oss.str());
    }
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
        }
        if (channel.Measurand.empty()) {
            add_error(report, field + ".Measurand must not be empty");
        }
        if (!std::isfinite(channel.Scale)) {
            add_error(report, field + ".Scale must be finite");
        }
        if (!std::isfinite(channel.Azimuth)
            || !(channel.Azimuth == -1.0
                 || (channel.Azimuth >= 0.0 && channel.Azimuth <= 360.0))) {
            add_error(report, field + ".Azimuth must be -1 or within [0, 360]");
        }
        for (double coord : channel.LocationXYZ) {
            if (!std::isfinite(coord)) {
                add_error(report, field + ".LocationXYZ must be finite");
                break;
            }
        }
    }

    const auto &data = metadata.DataInfo;
    if (data.NPTS <= 0) {
        add_error(report, "DataInfo.NPTS must be positive");
    }
    sampling_rate_or_error(report, data.DT);
    timestamp_or_error(report, data.StartTime);
    if (data.Corrected.empty()) {
        add_warning(report, "DataInfo.Corrected is empty");
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

    append_report(report, validate_metadata(file.metadata));
    if (!report.ok()) {
        return report;
    }

    if (file.packet.channel_count
        != static_cast<std::uint16_t>(
            file.metadata.InstrumentInfo.ChannelNum)) {
        std::ostringstream oss;
        oss << "Packet channel_count (" << file.packet.channel_count
            << ") does not match InstrumentInfo.ChannelNum ("
            << file.metadata.InstrumentInfo.ChannelNum << ")";
        add_error(report, oss.str());
    }
    if (file.packet.data_point_count
        != static_cast<std::uint32_t>(file.metadata.DataInfo.NPTS)) {
        std::ostringstream oss;
        oss << "Packet data_point_count (" << file.packet.data_point_count
            << ") does not match DataInfo.NPTS (" << file.metadata.DataInfo.NPTS
            << ")";
        add_error(report, oss.str());
    }

    const std::uint16_t metadata_sampling_rate =
        sampling_rate_or_error(report, file.metadata.DataInfo.DT);
    if (metadata_sampling_rate != 0
        && file.packet.sampling_rate != metadata_sampling_rate) {
        std::ostringstream oss;
        oss << "Packet sampling_rate (" << file.packet.sampling_rate
            << ") does not match DataInfo.DT-derived sampling rate ("
            << metadata_sampling_rate << ")";
        add_error(report, oss.str());
    }

    const std::uint64_t metadata_timestamp =
        timestamp_or_error(report, file.metadata.DataInfo.StartTime);
    if (metadata_timestamp != 0
        && file.packet.timestamp_ms != metadata_timestamp) {
        std::ostringstream oss;
        oss << "Packet timestamp_ms (" << file.packet.timestamp_ms
            << ") does not match DataInfo.StartTime (" << metadata_timestamp
            << ")";
        add_error(report, oss.str());
    }

    const std::size_t expected_values =
        static_cast<std::size_t>(file.packet.channel_count)
        * file.packet.data_point_count;
    if (file.channel_sequential_data.size() != expected_values) {
        std::ostringstream oss;
        oss << "Packet data size mismatch: expected " << expected_values
            << ", got " << file.channel_sequential_data.size();
        add_error(report, oss.str());
    }

    return report;
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
