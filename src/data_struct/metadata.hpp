// qREST_Data 文件存储格式中的元信息JSON

#ifndef QREST_DATA_METADATA_HPP
#define QREST_DATA_METADATA_HPP

#include <array>
#include <cstddef>
#include <string>
#include <vector>

#include "nlohmann/json.hpp"

namespace qrest_data
{

// 元信息结构体
class Metadata
{
public:
    // 构造函数
    Metadata() = default;

    Metadata(std::string json_str) { *this = from_bytes(json_str); }

    // --- 序列化 (内存对象 -> 二进制数据流) ---
    [[nodiscard]] std::string to_bytes(std::size_t dump = -1) const
    {
        nlohmann::json j;
        j["Header"] = Header;
        j["Version"] = Version;
        j["Units"] = Units;

        // BuildingInfo
        j["BuildingInfo"]["GeoLocation"]["Longitude"] =
            BuildingInfo.GeoLocation.Longitude;
        j["BuildingInfo"]["GeoLocation"]["Latitude"] =
            BuildingInfo.GeoLocation.Latitude;
        j["BuildingInfo"]["GeoLocation"]["NorthAngle"] =
            BuildingInfo.GeoLocation.NorthAngle;

        j["BuildingInfo"]["StructuralFootprint"]["Shape"] =
            BuildingInfo.StructuralFootprint.Shape;
        if (BuildingInfo.StructuralFootprint.Shape == "Circular")
        {
            j["BuildingInfo"]["StructuralFootprint"]["Parameters"]["Length"] =
                BuildingInfo.StructuralFootprint.Parameters.Length;
            j["BuildingInfo"]["StructuralFootprint"]["Parameters"]["Width"] =
                BuildingInfo.StructuralFootprint.Parameters.Width;
        }
        else if (BuildingInfo.StructuralFootprint.Shape == "Rectangular")
        {
            j["BuildingInfo"]["StructuralFootprint"]["Parameters"]["Radius"] =
                BuildingInfo.StructuralFootprint.Parameters.Radius;
        }
        else if (BuildingInfo.StructuralFootprint.Shape == "Polygon")
        {
            for (const auto &corner :
                 BuildingInfo.StructuralFootprint.Parameters.Corners)
            {
                j["BuildingInfo"]["StructuralFootprint"]["Parameters"]
                 ["Corners"]
                     .push_back(corner);
            }
        }

        j["BuildingInfo"]["StructuralFootprint"]["BoundingBox"]["MaxX"] =
            BuildingInfo.StructuralFootprint.BoundingBox.MaxX;
        j["BuildingInfo"]["StructuralFootprint"]["BoundingBox"]["MinX"] =
            BuildingInfo.StructuralFootprint.BoundingBox.MinX;
        j["BuildingInfo"]["StructuralFootprint"]["BoundingBox"]["MaxY"] =
            BuildingInfo.StructuralFootprint.BoundingBox.MaxY;
        j["BuildingInfo"]["StructuralFootprint"]["BoundingBox"]["MinY"] =
            BuildingInfo.StructuralFootprint.BoundingBox.MinY;

        j["BuildingInfo"]["ProjectName"] = BuildingInfo.ProjectName;
        j["BuildingInfo"]["StructuralType"] = BuildingInfo.StructuralType;
        j["BuildingInfo"]["ElevationNum"] = BuildingInfo.ElevationNum;
        j["BuildingInfo"]["Elevation"] = BuildingInfo.Elevation;

        // InstrumentInfo
        j["InstrumentInfo"]["Provider"] = InstrumentInfo.Provider;
        j["InstrumentInfo"]["ChannelNum"] = InstrumentInfo.ChannelNum;

        for (const auto &channel : InstrumentInfo.Channels)
        {
            nlohmann::json channel_json;
            channel_json["ChannelNo"] = channel.ChannelNo;
            channel_json["ChannelID"] = channel.ChannelID;
            channel_json["Measurand"] = channel.Measurand;
            channel_json["Scale"] = channel.Scale;
            channel_json["Azimuth"] = channel.Azimuth;
            channel_json["LocationXYZ"] = channel.LocationXYZ;
            j["InstrumentInfo"]["Channels"].push_back(channel_json);
        }

        // DataInfo
        j["DataInfo"]["EventName"] = DataInfo.EventName;
        j["DataInfo"]["StartTime"] = DataInfo.StartTime;
        j["DataInfo"]["NPTS"] = DataInfo.NPTS;
        j["DataInfo"]["DT"] = DataInfo.DT;
        j["DataInfo"]["Corrected"] = DataInfo.Corrected;

        return j.dump(dump);
    }

    // --- 反序列化 (二进制数据流 -> 内存对象) ---
    [[nodiscard]] static Metadata from_bytes(const std::string &json_str)
    {
        Metadata metadata;
        nlohmann::json j = nlohmann::json::parse(json_str);

        metadata.Header = j["Header"].get<std::string>();
        metadata.Version = j["Version"].get<std::array<int, 3>>();
        metadata.Units = j["Units"].get<std::array<std::string, 2>>();

        // BuildingInfo
        metadata.BuildingInfo.GeoLocation.Longitude =
            j["BuildingInfo"]["GeoLocation"]["Longitude"].get<double>();
        metadata.BuildingInfo.GeoLocation.Latitude =
            j["BuildingInfo"]["GeoLocation"]["Latitude"].get<double>();
        metadata.BuildingInfo.GeoLocation.NorthAngle =
            j["BuildingInfo"]["GeoLocation"]["NorthAngle"].get<double>();

        metadata.BuildingInfo.StructuralFootprint.Shape =
            j["BuildingInfo"]["StructuralFootprint"]["Shape"]
                .get<std::string>();
        if (metadata.BuildingInfo.StructuralFootprint.Shape == "Circle")
        {
            metadata.BuildingInfo.StructuralFootprint.Parameters.Length =
                j["BuildingInfo"]["StructuralFootprint"]["Parameters"]["Length"]
                    .get<double>();
            metadata.BuildingInfo.StructuralFootprint.Parameters.Width =
                j["BuildingInfo"]["StructuralFootprint"]["Parameters"]["Width"]
                    .get<double>();
        }
        else if (metadata.BuildingInfo.StructuralFootprint.Shape == "Rectangle")
        {
            metadata.BuildingInfo.StructuralFootprint.Parameters.Radius =
                j["BuildingInfo"]["StructuralFootprint"]["Parameters"]["Radius"]
                    .get<double>();
        }
        else if (metadata.BuildingInfo.StructuralFootprint.Shape == "Polygon")
        {
            for (const auto &corner : j["BuildingInfo"]["StructuralFootprint"]
                                       ["Parameters"]["Corners"])
            {
                metadata.BuildingInfo.StructuralFootprint.Parameters.Corners
                    .push_back(corner.get<std::array<double, 2>>());
            }
        }

        metadata.BuildingInfo.StructuralFootprint.BoundingBox.MaxX =
            j["BuildingInfo"]["StructuralFootprint"]["BoundingBox"]["MaxX"]
                .get<double>();
        metadata.BuildingInfo.StructuralFootprint.BoundingBox.MinX =
            j["BuildingInfo"]["StructuralFootprint"]["BoundingBox"]["MinX"]
                .get<double>();
        metadata.BuildingInfo.StructuralFootprint.BoundingBox.MaxY =
            j["BuildingInfo"]["StructuralFootprint"]["BoundingBox"]["MaxY"]
                .get<double>();
        metadata.BuildingInfo.StructuralFootprint.BoundingBox.MinY =
            j["BuildingInfo"]["StructuralFootprint"]["BoundingBox"]["MinY"]
                .get<double>();

        metadata.BuildingInfo.ProjectName =
            j["BuildingInfo"]["ProjectName"].get<std::string>();
        metadata.BuildingInfo.StructuralType =
            j["BuildingInfo"]["StructuralType"].get<std::string>();
        metadata.BuildingInfo.ElevationNum =
            j["BuildingInfo"]["ElevationNum"].get<int>();
        metadata.BuildingInfo.Elevation =
            j["BuildingInfo"]["Elevation"].get<std::vector<double>>();

        // InstrumentInfo
        metadata.InstrumentInfo.Provider =
            j["InstrumentInfo"]["Provider"].get<std::string>();
        metadata.InstrumentInfo.ChannelNum =
            j["InstrumentInfo"]["ChannelNum"].get<int>();
        for (const auto &channel_json : j["InstrumentInfo"]["Channels"])
        {
            Metadata::InstrumentInfoStruct::ChannelStruct channel;
            channel.ChannelNo = channel_json["ChannelNo"].get<int>();
            channel.ChannelID = channel_json["ChannelID"].get<std::string>();
            channel.Measurand = channel_json["Measurand"].get<std::string>();
            channel.Scale = channel_json["Scale"].get<double>();
            channel.Azimuth = channel_json["Azimuth"].get<double>();
            channel.LocationXYZ =
                channel_json["LocationXYZ"].get<std::array<double, 3>>();
            metadata.InstrumentInfo.Channels.push_back(channel);
        }

        // DataInfo
        metadata.DataInfo.EventName =
            j["DataInfo"]["EventName"].get<std::string>();
        metadata.DataInfo.StartTime =
            j["DataInfo"]["StartTime"].get<std::string>();
        metadata.DataInfo.NPTS = j["DataInfo"]["NPTS"].get<int>();
        metadata.DataInfo.DT = j["DataInfo"]["DT"].get<double>();
        metadata.DataInfo.Corrected =
            j["DataInfo"]["Corrected"].get<std::string>();

        return metadata;
    }

    struct BuildingInfoStruct
    {
        struct GeoLocationStruct
        {
            double Longitude{};
            double Latitude{};
            double NorthAngle{};
        } GeoLocation;

        struct StructuralFootprintStruct
        {
            struct ParametersStruct
            {
                double Length{};
                double Width{};
                double Radius{};
                std::vector<std::array<double, 2>> Corners{};
            } Parameters;

            struct BoundingBoxStruct
            {
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

    struct InstrumentInfoStruct
    {
        struct ChannelStruct
        {
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

    struct DataInfoStruct
    {
        std::string EventName{};
        std::string StartTime{};
        int NPTS{};
        double DT{};
        std::string Corrected{};
    };

public:
    // 元数据取用频繁，故设计为公有成员变量，且变量名和JSON字段名保持一致
    // 根节点
    std::string Header{"qREST_DATA"};           // 文件标识
    std::array<int, 3> Version{1, 0, 0};        // 版本号
    std::array<std::string, 2> Units{"m", "s"}; // 单位
    BuildingInfoStruct BuildingInfo{};
    InstrumentInfoStruct InstrumentInfo{};
    DataInfoStruct DataInfo{};
};
} // namespace qrest_data

#endif