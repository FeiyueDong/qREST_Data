// qREST_Data 文件存储格式中的元信息JSON
#ifndef QREST_DATA_METADATA_HPP
#define QREST_DATA_METADATA_HPP

#include <array>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

#include "nlohmann/json.hpp"

namespace qrest_data {
class Metadata {
public:
    struct BuildingInfoStruct {
        struct GeoLocationStruct {
            double Longitude{};
            double Latitude{};
            double NorthAngle{};
        } GeoLocation;

        struct StructuralFootprintStruct {
            struct ParametersStruct {
                double Length{};
                double Width{};
                double Radius{};
                std::vector<std::array<double, 2>> Corners{};
            } Parameters;

            struct BoundingBoxStruct {
                double MaxX{};
                double MinX{};
                double MaxY{};
                double MinY{};
            } BoundingBox;

            std::string Shape{};
        } StructuralFootprint;

        std::string ProjectName{};
        std::string StructuralType{};
        int ElevationNum{};
        std::vector<double> Elevation{};
    };

    struct InstrumentInfoStruct {
        struct ChannelStruct {
            int ChannelNo{};
            std::string ChannelID{};
            std::string Measurand{};
            double Scale{};
            double Azimuth{};
            std::array<double, 3> LocationXYZ{};
        };

        std::string Provider{};
        int ChannelNum{};
        std::vector<ChannelStruct> Channels{};
    };

    struct DataInfoStruct {
        std::string EventName{};
        std::string StartTime{};
        int NPTS{};
        double DT{};
        std::string Corrected{};
    };

public:
    // 元数据取用频繁，故设计为公有成员变量，且变量名和JSON字段名保持一致
    std::string Header{"qREST_DATA"};           // 文件标识
    std::array<int, 3> Version{1, 0, 0};        // 版本号
    std::array<std::string, 2> Units{"m", "s"}; // 单位
    BuildingInfoStruct BuildingInfo{};
    InstrumentInfoStruct InstrumentInfo{};
    DataInfoStruct DataInfo{};

public:
    Metadata() = default;

    explicit Metadata(std::string_view json_str) {
        *this = from_bytes(json_str);
    }

    [[nodiscard]] std::string to_bytes(int indent = -1) const;
    [[nodiscard]] static Metadata from_bytes(std::string_view json_str);
};
// --- GeoLocationStruct ---
inline void
to_json(nlohmann::json &j,
        const Metadata::BuildingInfoStruct::GeoLocationStruct &loc) {
    j = nlohmann::json{{"Longitude", loc.Longitude},
                       {"Latitude", loc.Latitude},
                       {"NorthAngle", loc.NorthAngle}};
}
inline void from_json(const nlohmann::json &j,
                      Metadata::BuildingInfoStruct::GeoLocationStruct &loc) {
    j.at("Longitude").get_to(loc.Longitude);
    j.at("Latitude").get_to(loc.Latitude);
    j.at("NorthAngle").get_to(loc.NorthAngle);
}

// --- StructuralFootprintStruct ---
inline void
to_json(nlohmann::json &j,
        const Metadata::BuildingInfoStruct::StructuralFootprintStruct &sf) {
    j["Shape"] = sf.Shape;
    if (sf.Shape == "Circular") {
        j["Parameters"]["Radius"] = sf.Parameters.Radius;
    } else if (sf.Shape == "Rectangular") {
        j["Parameters"]["Length"] = sf.Parameters.Length;
        j["Parameters"]["Width"] = sf.Parameters.Width;
    } else if (sf.Shape == "Polygon") {
        j["Parameters"]["Corners"] = sf.Parameters.Corners;
    }
    j["BoundingBox"] = nlohmann::json{{"MaxX", sf.BoundingBox.MaxX},
                                      {"MinX", sf.BoundingBox.MinX},
                                      {"MaxY", sf.BoundingBox.MaxY},
                                      {"MinY", sf.BoundingBox.MinY}};
}
inline void
from_json(const nlohmann::json &j,
          Metadata::BuildingInfoStruct::StructuralFootprintStruct &sf) {
    j.at("Shape").get_to(sf.Shape);
    const auto &params = j.at("Parameters");

    if (sf.Shape == "Circular") {
        params.at("Radius").get_to(sf.Parameters.Radius);
    } else if (sf.Shape == "Rectangular") {
        params.at("Length").get_to(sf.Parameters.Length);
        params.at("Width").get_to(sf.Parameters.Width);
    } else if (sf.Shape == "Polygon") {
        params.at("Corners").get_to(sf.Parameters.Corners);
    } else {
        throw std::invalid_argument("Unknown StructuralFootprint Shape: "
                                    + sf.Shape);
    }

    const auto &bbox = j.at("BoundingBox");
    bbox.at("MaxX").get_to(sf.BoundingBox.MaxX);
    bbox.at("MinX").get_to(sf.BoundingBox.MinX);
    bbox.at("MaxY").get_to(sf.BoundingBox.MaxY);
    bbox.at("MinY").get_to(sf.BoundingBox.MinY);
}

// --- BuildingInfoStruct ---
inline void to_json(nlohmann::json &j,
                    const Metadata::BuildingInfoStruct &info) {
    j = nlohmann::json{{"GeoLocation", info.GeoLocation},
                       {"StructuralFootprint", info.StructuralFootprint},
                       {"ProjectName", info.ProjectName},
                       {"StructuralType", info.StructuralType},
                       {"ElevationNum", info.ElevationNum},
                       {"Elevation", info.Elevation}};
}
inline void from_json(const nlohmann::json &j,
                      Metadata::BuildingInfoStruct &info) {
    j.at("GeoLocation").get_to(info.GeoLocation);
    j.at("StructuralFootprint").get_to(info.StructuralFootprint);
    j.at("ProjectName").get_to(info.ProjectName);
    j.at("StructuralType").get_to(info.StructuralType);
    j.at("ElevationNum").get_to(info.ElevationNum);
    j.at("Elevation").get_to(info.Elevation);
}

// --- ChannelStruct ---
inline void to_json(nlohmann::json &j,
                    const Metadata::InstrumentInfoStruct::ChannelStruct &ch) {
    j = nlohmann::json{{"ChannelNo", ch.ChannelNo},
                       {"ChannelID", ch.ChannelID},
                       {"Measurand", ch.Measurand},
                       {"Scale", ch.Scale},
                       {"Azimuth", ch.Azimuth},
                       {"LocationXYZ", ch.LocationXYZ}};
}
inline void from_json(const nlohmann::json &j,
                      Metadata::InstrumentInfoStruct::ChannelStruct &ch) {
    j.at("ChannelNo").get_to(ch.ChannelNo);
    j.at("ChannelID").get_to(ch.ChannelID);
    j.at("Measurand").get_to(ch.Measurand);
    j.at("Scale").get_to(ch.Scale);
    j.at("Azimuth").get_to(ch.Azimuth);
    j.at("LocationXYZ").get_to(ch.LocationXYZ);
}

// --- InstrumentInfoStruct ---
inline void to_json(nlohmann::json &j,
                    const Metadata::InstrumentInfoStruct &info) {
    j = nlohmann::json{{"Provider", info.Provider},
                       {"ChannelNum", info.ChannelNum},
                       {"Channels", info.Channels}};
}
inline void from_json(const nlohmann::json &j,
                      Metadata::InstrumentInfoStruct &info) {
    j.at("Provider").get_to(info.Provider);
    j.at("ChannelNum").get_to(info.ChannelNum);
    j.at("Channels").get_to(info.Channels); // 自动解析 std::vector
}

// --- DataInfoStruct ---
inline void to_json(nlohmann::json &j, const Metadata::DataInfoStruct &info) {
    j = nlohmann::json{{"EventName", info.EventName},
                       {"StartTime", info.StartTime},
                       {"NPTS", info.NPTS},
                       {"DT", info.DT},
                       {"Corrected", info.Corrected}};
}
inline void from_json(const nlohmann::json &j, Metadata::DataInfoStruct &info) {
    j.at("EventName").get_to(info.EventName);
    j.at("StartTime").get_to(info.StartTime);
    j.at("NPTS").get_to(info.NPTS);
    j.at("DT").get_to(info.DT);
    j.at("Corrected").get_to(info.Corrected);
}

// --- 顶级 Metadata 类 ---
inline void to_json(nlohmann::json &j, const Metadata &m) {
    j["Header"] = m.Header;
    j["Version"] = m.Version;
    j["Units"] = m.Units;

    // 按需添加 BuildingInfo
    j["BuildingInfo"] = m.BuildingInfo;
    j["InstrumentInfo"] = m.InstrumentInfo;
    j["DataInfo"] = m.DataInfo;
}

inline void from_json(const nlohmann::json &j, Metadata &m) {
    j.at("Header").get_to(m.Header);
    j.at("Version").get_to(m.Version);
    j.at("Units").get_to(m.Units);

    if (j.contains("BuildingInfo")) {
        j.at("BuildingInfo").get_to(m.BuildingInfo);
    }

    j.at("InstrumentInfo").get_to(m.InstrumentInfo);
    j.at("DataInfo").get_to(m.DataInfo);
}

inline std::string Metadata::to_bytes(int indent) const {
    nlohmann::json j = *this;
    return j.dump(indent);
}

inline Metadata Metadata::from_bytes(std::string_view json_str) {
    return nlohmann::json::parse(json_str).get<Metadata>();
}

} // namespace qrest_data

#endif