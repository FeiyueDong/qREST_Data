// qREST_Data 文件存储格式中的元信息JSON
#ifndef QREST_DATA_METADATA_HPP
#define QREST_DATA_METADATA_HPP

#include <array>
#include <initializer_list>
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
            nlohmann::json Extension = nlohmann::json::object();
        } GeoLocation;

        struct StructuralFootprintStruct {
            struct ParametersStruct {
                double Length{};
                double Width{};
                double Radius{};
                std::vector<std::array<double, 2>> Corners{};
                nlohmann::json Extension = nlohmann::json::object();
            } Parameters;

            struct BoundingBoxStruct {
                double MaxX{};
                double MinX{};
                double MaxY{};
                double MinY{};
                nlohmann::json Extension = nlohmann::json::object();
            } BoundingBox;

            std::string Shape{};
            nlohmann::json Extension = nlohmann::json::object();
        } StructuralFootprint;

        std::string ProjectName{};
        std::string StructuralType{};
        int ElevationNum{};
        std::vector<double> Elevation{};
        nlohmann::json Extension = nlohmann::json::object();
    };

    struct InstrumentInfoStruct {
        struct ChannelStruct {
            int ChannelNo{};
            std::string ChannelID{};
            std::string DeviceType{};
            std::string Measurand{};
            double Scale{};
            double Azimuth{};
            std::array<double, 3> LocationXYZ{};
            nlohmann::json Extension = nlohmann::json::object();
        };

        std::string Provider{};
        int ChannelNum{};
        std::vector<ChannelStruct> Channels{};
        nlohmann::json Extension = nlohmann::json::object();
    };

    struct DataInfoStruct {
        std::string EventName{};
        std::string StartTime{};
        int NPTS{};
        double DT{};
        double Frequency{};
        std::string Corrected{};
        nlohmann::json Extension = nlohmann::json::object();
    };

public:
    // 元数据取用频繁，故设计为公有成员变量，且变量名和JSON字段名保持一致
    std::string Header{"qREST_DATA"};           // 文件标识
    std::array<int, 3> Version{1, 0, 0};        // 版本号
    std::array<std::string, 2> Units{"m", "s"}; // 单位
    BuildingInfoStruct BuildingInfo{};
    InstrumentInfoStruct InstrumentInfo{};
    DataInfoStruct DataInfo{};
    nlohmann::json Extension = nlohmann::json::object();

public:
    Metadata() = default;

    explicit Metadata(std::string_view json_str) {
        *this = from_bytes(json_str);
    }

    [[nodiscard]] std::string to_bytes(int indent = -1) const;
    [[nodiscard]] static Metadata from_bytes(std::string_view json_str);
};
inline nlohmann::json
copy_unknown_fields(const nlohmann::json &j,
                    std::initializer_list<std::string_view> known_fields) {
    nlohmann::json extension = nlohmann::json::object();
    if (!j.is_object()) {
        return extension;
    }

    for (auto it = j.begin(); it != j.end(); ++it) {
        bool known = false;
        for (std::string_view field : known_fields) {
            if (it.key() == field) {
                known = true;
                break;
            }
        }
        if (!known) {
            extension[it.key()] = it.value();
        }
    }
    return extension;
}

inline nlohmann::json object_with_extension(const nlohmann::json &extension) {
    return extension.is_object() ? extension : nlohmann::json::object();
}

// --- GeoLocationStruct ---
inline void
to_json(nlohmann::json &j,
        const Metadata::BuildingInfoStruct::GeoLocationStruct &loc) {
    j = object_with_extension(loc.Extension);
    j["Longitude"] = loc.Longitude;
    j["Latitude"] = loc.Latitude;
    j["NorthAngle"] = loc.NorthAngle;
}
inline void from_json(const nlohmann::json &j,
                      Metadata::BuildingInfoStruct::GeoLocationStruct &loc) {
    loc.Extension =
        copy_unknown_fields(j, {"Longitude", "Latitude", "NorthAngle"});
    j.at("Longitude").get_to(loc.Longitude);
    j.at("Latitude").get_to(loc.Latitude);
    j.at("NorthAngle").get_to(loc.NorthAngle);
}

// --- StructuralFootprintStruct ---
inline void
to_json(nlohmann::json &j,
        const Metadata::BuildingInfoStruct::StructuralFootprintStruct &sf) {
    j = object_with_extension(sf.Extension);
    j["Shape"] = sf.Shape;
    j["Parameters"] = object_with_extension(sf.Parameters.Extension);
    if (sf.Shape == "Circular") {
        j["Parameters"]["Radius"] = sf.Parameters.Radius;
    } else if (sf.Shape == "Rectangular") {
        j["Parameters"]["Length"] = sf.Parameters.Length;
        j["Parameters"]["Width"] = sf.Parameters.Width;
    } else if (sf.Shape == "Polygon") {
        j["Parameters"]["Corners"] = sf.Parameters.Corners;
    }
    j["BoundingBox"] = object_with_extension(sf.BoundingBox.Extension);
    j["BoundingBox"]["MaxX"] = sf.BoundingBox.MaxX;
    j["BoundingBox"]["MinX"] = sf.BoundingBox.MinX;
    j["BoundingBox"]["MaxY"] = sf.BoundingBox.MaxY;
    j["BoundingBox"]["MinY"] = sf.BoundingBox.MinY;
}
inline void
from_json(const nlohmann::json &j,
          Metadata::BuildingInfoStruct::StructuralFootprintStruct &sf) {
    sf.Extension =
        copy_unknown_fields(j, {"Shape", "Parameters", "BoundingBox"});
    j.at("Shape").get_to(sf.Shape);
    const auto &params = j.at("Parameters");
    sf.Parameters.Extension =
        copy_unknown_fields(params, {"Length", "Width", "Radius", "Corners"});

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
    sf.BoundingBox.Extension =
        copy_unknown_fields(bbox, {"MaxX", "MinX", "MaxY", "MinY"});
    bbox.at("MaxX").get_to(sf.BoundingBox.MaxX);
    bbox.at("MinX").get_to(sf.BoundingBox.MinX);
    bbox.at("MaxY").get_to(sf.BoundingBox.MaxY);
    bbox.at("MinY").get_to(sf.BoundingBox.MinY);
}

// --- BuildingInfoStruct ---
inline void to_json(nlohmann::json &j,
                    const Metadata::BuildingInfoStruct &info) {
    j = object_with_extension(info.Extension);
    j["GeoLocation"] = info.GeoLocation;
    j["StructuralFootprint"] = info.StructuralFootprint;
    j["ProjectName"] = info.ProjectName;
    j["StructuralType"] = info.StructuralType;
    j["ElevationNum"] = info.ElevationNum;
    j["Elevation"] = info.Elevation;
}
inline void from_json(const nlohmann::json &j,
                      Metadata::BuildingInfoStruct &info) {
    info.Extension = copy_unknown_fields(j,
                                         {"GeoLocation",
                                          "StructuralFootprint",
                                          "ProjectName",
                                          "StructuralType",
                                          "ElevationNum",
                                          "Elevation"});
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
    j = object_with_extension(ch.Extension);
    j["ChannelNo"] = ch.ChannelNo;
    j["ChannelID"] = ch.ChannelID;
    j["DeviceType"] = ch.DeviceType;
    j["Measurand"] = ch.Measurand;
    j["Scale"] = ch.Scale;
    j["Azimuth"] = ch.Azimuth;
    j["LocationXYZ"] = ch.LocationXYZ;
}
inline void from_json(const nlohmann::json &j,
                      Metadata::InstrumentInfoStruct::ChannelStruct &ch) {
    ch.Extension = copy_unknown_fields(j,
                                       {"ChannelNo",
                                        "ChannelID",
                                        "DeviceType",
                                        "Measurand",
                                        "Scale",
                                        "Azimuth",
                                        "LocationXYZ"});
    j.at("ChannelNo").get_to(ch.ChannelNo);
    j.at("ChannelID").get_to(ch.ChannelID);
    ch.DeviceType = j.value("DeviceType", std::string{"Unknown"});
    j.at("Measurand").get_to(ch.Measurand);
    j.at("Scale").get_to(ch.Scale);
    j.at("Azimuth").get_to(ch.Azimuth);
    j.at("LocationXYZ").get_to(ch.LocationXYZ);
}

// --- InstrumentInfoStruct ---
inline void to_json(nlohmann::json &j,
                    const Metadata::InstrumentInfoStruct &info) {
    j = object_with_extension(info.Extension);
    j["Provider"] = info.Provider;
    j["ChannelNum"] = info.ChannelNum;
    j["Channels"] = info.Channels;
}
inline void from_json(const nlohmann::json &j,
                      Metadata::InstrumentInfoStruct &info) {
    info.Extension =
        copy_unknown_fields(j, {"Provider", "ChannelNum", "Channels"});
    j.at("Provider").get_to(info.Provider);
    j.at("ChannelNum").get_to(info.ChannelNum);
    j.at("Channels").get_to(info.Channels); // 自动解析 std::vector
}

// --- DataInfoStruct ---
inline void to_json(nlohmann::json &j, const Metadata::DataInfoStruct &info) {
    j = object_with_extension(info.Extension);
    j["EventName"] = info.EventName;
    j["StartTime"] = info.StartTime;
    j["NPTS"] = info.NPTS;
    j["DT"] = info.DT;
    j["Corrected"] = info.Corrected;
}
inline void from_json(const nlohmann::json &j, Metadata::DataInfoStruct &info) {
    info.Extension = copy_unknown_fields(
        j, {"EventName", "StartTime", "NPTS", "DT", "Corrected"});
    j.at("EventName").get_to(info.EventName);
    j.at("StartTime").get_to(info.StartTime);
    j.at("NPTS").get_to(info.NPTS);
    j.at("DT").get_to(info.DT);
    j.at("Corrected").get_to(info.Corrected);
    info.Frequency = static_cast<int>(1.0 / info.DT + 0.5); // 四舍五入取整
}

// --- 顶级 Metadata 类 ---
inline void to_json(nlohmann::json &j, const Metadata &m) {
    j = object_with_extension(m.Extension);
    j["Header"] = m.Header;
    j["Version"] = m.Version;
    j["Units"] = m.Units;

    // 按需添加 BuildingInfo
    j["BuildingInfo"] = m.BuildingInfo;
    j["InstrumentInfo"] = m.InstrumentInfo;
    j["DataInfo"] = m.DataInfo;
}

inline void from_json(const nlohmann::json &j, Metadata &m) {
    m.Extension = copy_unknown_fields(j,
                                      {"Header",
                                       "Version",
                                       "Units",
                                       "BuildingInfo",
                                       "InstrumentInfo",
                                       "DataInfo"});
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
