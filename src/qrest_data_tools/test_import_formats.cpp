#include "external_import.hpp"
#include "modified_mseed_export.hpp"
#include "qrest_file.hpp"
#include "tdms_export.hpp"

#include <algorithm>
#include <cmath>
#include <exception>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

template <typename T, typename U>
void require_equal(const T &actual, const U &expected, const char *message) {
    if (actual != expected) {
        throw std::runtime_error(message);
    }
}

void require_near(double actual,
                  double expected,
                  double tolerance,
                  const char *message) {
    if (std::fabs(actual - expected) > tolerance) {
        throw std::runtime_error(message);
    }
}

template <typename T>
void require_not_empty(const T &container, const char *message) {
    if (container.empty()) {
        throw std::runtime_error(message);
    }
}

std::filesystem::path project_path(const char *relative) {
    return std::filesystem::path(QREST_DATA_PROJECT_DIR) / relative;
}

qrest_data::Metadata
make_metadata(const std::vector<std::string> &labels, int npts, double dt) {
    qrest_data::Metadata metadata;
    metadata.BuildingInfo.ProjectName = "ImportFormatTest";
    metadata.BuildingInfo.StructuralType = "UNKNOWN";
    metadata.BuildingInfo.StructuralFootprint.Shape = "Rectangular";
    metadata.InstrumentInfo.Provider = "TestProvider";
    metadata.InstrumentInfo.ChannelNum = static_cast<int>(labels.size());
    metadata.InstrumentInfo.Channels.reserve(labels.size());
    for (std::size_t i = 0; i < labels.size(); ++i) {
        qrest_data::Metadata::InstrumentInfoStruct::ChannelStruct channel;
        channel.ChannelNo = static_cast<int>(i + 1);
        channel.ChannelID = labels[i];
        channel.Measurand = "Acceleration";
        channel.Scale = 1.0;
        channel.Azimuth = -1.0;
        channel.LocationXYZ = {0.0, 0.0, 0.0};
        metadata.InstrumentInfo.Channels.push_back(channel);
    }
    metadata.DataInfo.EventName = "ImportFormatTest";
    metadata.DataInfo.StartTime = "2026-08-20T03:00:00.000Z";
    metadata.DataInfo.NPTS = npts;
    metadata.DataInfo.DT = dt;
    metadata.DataInfo.Corrected = "NULL";
    return metadata;
}

void assert_round_trip_qrest(const qrest_data::tools::ExternalDataset &dataset,
                             const qrest_data::Metadata &metadata,
                             const char *filename) {
    qrest_data::tools::require_external_dataset_compatibility(dataset,
                                                              metadata);

    const auto qrest_path =
        (std::filesystem::temp_directory_path() / filename).string();
    qrest_data::tools::write_qrest_file(
        qrest_path, metadata, dataset.channel_sequential_data, 1, 1);

    const auto qrest = qrest_data::tools::read_qrest_file(qrest_path);
    require_equal(qrest.metadata.InstrumentInfo.ChannelNum,
                  metadata.InstrumentInfo.ChannelNum,
                  "qREST round-trip channel count");
    require_equal(qrest.metadata.DataInfo.NPTS,
                  metadata.DataInfo.NPTS,
                  "qREST round-trip sample count");
    require_equal(qrest.channel_sequential_data.size(),
                  dataset.channel_sequential_data.size(),
                  "qREST round-trip value count");
    require_near(qrest.channel_sequential_data.front(),
                 dataset.channel_sequential_data.front(),
                 1e-12,
                 "qREST round-trip first value");
    require_near(qrest.channel_sequential_data.back(),
                 dataset.channel_sequential_data.back(),
                 1e-12,
                 "qREST round-trip last value");
    std::filesystem::remove(qrest_path);
}

void test_mseed_sample() {
    namespace mseed = qrest_data::tools::mseed;

    const auto groups = mseed::load_channel_groups(
        project_path("resource/dev/data/WH.XY001-2024-01-01-15-15-00.mseed")
            .string());

    require_equal(groups.size(), std::size_t{1}, "MiniSEED group count");
    const auto &group = groups.front();
    require_equal(group.network, std::string{"WH"}, "MiniSEED network");
    require_equal(group.station, std::string{"XY001"}, "MiniSEED station");
    require_equal(group.location, std::string{"40"}, "MiniSEED location");
    require_equal(
        group.channels.size(), std::size_t{3}, "MiniSEED channel count");
    require_equal(
        group.sample_count(), std::size_t{240000}, "MiniSEED sample count");
    require_equal(
        group.dimension, mseed::Dimension::Acceleration, "MiniSEED dimension");
    require_near(group.sample_rate_hz, 200.0, 1e-12, "MiniSEED sample rate");
    require_equal(mseed::physical_unit_name(group.dimension),
                  std::string{"m/s^2"},
                  "MiniSEED unit");

    const auto channel = [&](const char *name) -> const mseed::ChannelSeries & {
        const auto it = std::find_if(group.channels.begin(),
                                     group.channels.end(),
                                     [&](const mseed::ChannelSeries &series) {
                                         return series.channel == name;
                                     });
        if (it == group.channels.end()) {
            throw std::runtime_error(std::string{"MiniSEED missing channel "}
                                     + name);
        }
        require_not_empty(it->values, "MiniSEED channel is empty");
        return *it;
    };

    const auto &north = channel("EIN");
    const auto &east = channel("EIE");
    const auto &vertical = channel("EIZ");
    require_near(north.values.front(), -6.0e-05, 1e-12, "MiniSEED N first");
    require_near(east.values.front(), -1.8e-04, 1e-12, "MiniSEED E first");
    require_near(vertical.values.front(), -6.0e-05, 1e-12, "MiniSEED Z first");
    require_near(north.values.back(), 0.0, 1e-12, "MiniSEED N last");
    require_near(east.values.back(), 4.0e-05, 1e-12, "MiniSEED E last");
    require_near(vertical.values.back(), -1.2e-04, 1e-12, "MiniSEED Z last");

    const auto imported = qrest_data::tools::load_mseed_dataset(
        project_path("resource/dev/data/WH.XY001-2024-01-01-15-15-00.mseed")
            .string());
    require_equal(
        imported.channel_count, std::size_t{3}, "Imported MiniSEED channels");
    require_equal(imported.sample_count,
                  std::size_t{240000},
                  "Imported MiniSEED samples");
    require_equal(
        imported.channel_labels.front(), std::string{"EIN"}, "MiniSEED label");
    require_near(imported.channel_sequential_data.front(),
                 -6.0e-05,
                 1e-12,
                 "Imported MiniSEED first value");
    require_near(imported.channel_sequential_data.back(),
                 -1.2e-04,
                 1e-12,
                 "Imported MiniSEED last value");

    assert_round_trip_qrest(imported,
                            make_metadata({"EIN", "EIE", "EIZ"}, 240000, 0.005),
                            "qrest_data_tools_mseed_import_test.qrest");
}

void test_tdms_sample() {
    namespace tdms = qrest_data::tools::tdms;

    const auto dataset = tdms::load_dataset(
        project_path("resource/dev/data/S01_20260820_110000_000_SIT_PYL06.tdms")
            .string());

    require_equal(dataset.name,
                  std::string{"S01_20260820_110000_000_SIT_PYL06"},
                  "TDMS dataset name");
    require_near(dataset.sample_rate_hz, 100.0, 1e-12, "TDMS sample rate");
    require_equal(
        dataset.north_counts.size(), std::size_t{360000}, "TDMS N count");
    require_equal(
        dataset.east_counts.size(), std::size_t{360000}, "TDMS E count");
    require_equal(
        dataset.vertical_counts.size(), std::size_t{360000}, "TDMS Z count");
    require_equal(
        dataset.timestamps.size(), std::size_t{360000}, "TDMS timestamp count");
    require_near(dataset.selected_sensitivity_raw,
                 10000000.0,
                 1e-6,
                 "TDMS selected sensitivity");
    require_equal(
        dataset.north_counts.front(), std::int32_t{3}, "TDMS N first");
    require_equal(
        dataset.east_counts.front(), std::int32_t{-5}, "TDMS E first");
    require_equal(
        dataset.vertical_counts.front(), std::int32_t{-18}, "TDMS Z first");
    require_equal(dataset.north_counts.back(), std::int32_t{18}, "TDMS N last");
    require_equal(dataset.east_counts.back(), std::int32_t{-12}, "TDMS E last");
    require_equal(
        dataset.vertical_counts.back(), std::int32_t{6}, "TDMS Z last");
    require_equal(tdms::format_timestamp_utc(dataset.timestamps.front(), 6),
                  std::string{"2026-08-20T03:00:00.000000Z"},
                  "TDMS first timestamp");
    require_equal(tdms::format_timestamp_utc(dataset.timestamps.back(), 6),
                  std::string{"2026-08-20T03:59:59.990000Z"},
                  "TDMS last timestamp");

    const auto imported = qrest_data::tools::load_tdms_dataset(
        project_path("resource/dev/data/S01_20260820_110000_000_SIT_PYL06.tdms")
            .string());
    require_equal(
        imported.channel_count, std::size_t{3}, "Imported TDMS channels");
    require_equal(
        imported.sample_count, std::size_t{360000}, "Imported TDMS samples");
    require_near(imported.channel_sequential_data.front(),
                 3.0e-03,
                 1e-12,
                 "Imported TDMS first value");
    require_near(imported.channel_sequential_data.back(),
                 6.0e-03,
                 1e-12,
                 "Imported TDMS last value");

    assert_round_trip_qrest(imported,
                            make_metadata({"N", "E", "Z"}, 360000, 0.01),
                            "qrest_data_tools_tdms_import_test.qrest");
}

void test_hdf5_bridge() {
    const auto qrest = qrest_data::tools::read_qrest_file(
        project_path("resource/wuhan/wuhan.qrest").string());
    const auto hdf5_path = (std::filesystem::temp_directory_path()
                            / "qrest_data_tools_import_formats_test.h5")
                               .string();

    qrest_data::tools::write_hdf5_dataset(
        hdf5_path, qrest.metadata, qrest.channel_sequential_data);

    qrest_data::Metadata metadata;
    const auto imported =
        qrest_data::tools::load_hdf5_dataset(hdf5_path, &metadata);
    require_equal(
        imported.channel_count,
        static_cast<std::size_t>(qrest.metadata.InstrumentInfo.ChannelNum),
        "Imported HDF5 channel count");
    require_equal(imported.sample_count,
                  static_cast<std::size_t>(qrest.metadata.DataInfo.NPTS),
                  "Imported HDF5 sample count");
    require_equal(imported.channel_sequential_data.size(),
                  qrest.channel_sequential_data.size(),
                  "Imported HDF5 value count");
    require_near(imported.channel_sequential_data.front(),
                 qrest.channel_sequential_data.front(),
                 1e-12,
                 "Imported HDF5 first value");
    require_near(imported.channel_sequential_data.back(),
                 qrest.channel_sequential_data.back(),
                 1e-12,
                 "Imported HDF5 last value");
    require_equal(metadata.BuildingInfo.ProjectName,
                  qrest.metadata.BuildingInfo.ProjectName,
                  "Imported HDF5 metadata");
    std::filesystem::remove(hdf5_path);
}

} // namespace

int main() {
    try {
        test_mseed_sample();
        test_tdms_sample();
        test_hdf5_bridge();
        std::cout << "qREST import format tests passed.\n";
        return 0;
    } catch (const std::exception &e) {
        std::cerr << "Error: " << e.what() << '\n';
        return 1;
    }
}
