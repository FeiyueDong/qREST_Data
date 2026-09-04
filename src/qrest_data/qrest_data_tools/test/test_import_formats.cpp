#include "../core/external_import.hpp"
#include "../core/qrest_file.hpp"
#include "../core/validation.hpp"
#include "../formats/mseed/modified_mseed_export.hpp"
#include "../formats/tdms/tdms_export.hpp"

#include <algorithm>
#include <cmath>
#include <exception>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

#ifndef QREST_DATA_PROJECT_DIR
#define QREST_DATA_PROJECT_DIR "."
#endif

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
    metadata.BuildingInfo.StructuralFootprint.Parameters.Length = 1.0;
    metadata.BuildingInfo.StructuralFootprint.Parameters.Width = 1.0;
    metadata.BuildingInfo.Elevation = {0.0};
    metadata.BuildingInfo.ElevationNum = 1;
    metadata.InstrumentInfo.Provider = "TestProvider";
    metadata.InstrumentInfo.ChannelNum = static_cast<int>(labels.size());
    metadata.InstrumentInfo.Channels.reserve(labels.size());
    for (std::size_t i = 0; i < labels.size(); ++i) {
        qrest_data::Metadata::InstrumentInfoStruct::ChannelStruct channel;
        channel.ChannelNo = static_cast<int>(i + 1);
        channel.ChannelID = labels[i];
        channel.DeviceType = "S05";
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
    const auto sample_path =
        project_path("resource/wuhan_mseed/WH.XY001-2024-01-01-15-15-00.mseed");

    const auto groups = mseed::load_channel_groups(sample_path.string());

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

    const auto imported =
        qrest_data::tools::load_mseed_dataset(sample_path.string());
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

    const auto mapped = qrest_data::tools::load_mseed_collection(
        sample_path.string(), make_metadata({"X1", "Y1", "Z1"}, 240000, 0.005));
    require_equal(
        mapped.channel_labels[0], std::string{"X1"}, "Mapped MiniSEED X label");
    require_equal(
        mapped.channel_labels[1], std::string{"Y1"}, "Mapped MiniSEED Y label");
    require_equal(
        mapped.channel_labels[2], std::string{"Z1"}, "Mapped MiniSEED Z label");
    require_near(mapped.channel_sequential_data[0],
                 -6.0e-05,
                 1e-12,
                 "Mapped MiniSEED first source channel -> X1");
    require_near(mapped.channel_sequential_data[240000],
                 -1.8e-04,
                 1e-12,
                 "Mapped MiniSEED second source channel -> Y1");
    require_near(mapped.channel_sequential_data[480000],
                 -6.0e-05,
                 1e-12,
                 "Mapped MiniSEED EIZ -> Z1");

    const auto temp_dir =
        std::filesystem::temp_directory_path() / "qrest_data_mseed_dir_test";
    std::filesystem::remove_all(temp_dir);
    std::filesystem::create_directories(temp_dir);
    std::filesystem::copy_file(sample_path, temp_dir / "A.mseed");
    std::filesystem::copy_file(sample_path, temp_dir / "B.mseed");

    const auto mapped_dir = qrest_data::tools::load_mseed_collection(
        temp_dir.string(),
        make_metadata({"X1", "Y1", "Z1", "X2", "Y2", "Z2"}, 240000, 0.005));
    require_equal(mapped_dir.channel_count,
                  std::size_t{6},
                  "Mapped MiniSEED directory channel count");
    require_equal(mapped_dir.channel_labels[3],
                  std::string{"X2"},
                  "Mapped MiniSEED directory filename order");
    require_near(mapped_dir.channel_sequential_data[3 * 240000],
                 -6.0e-05,
                 1e-12,
                 "Mapped MiniSEED directory first channel of second file");
    std::filesystem::remove_all(temp_dir);
}

void test_tdms_sample() {
    namespace tdms = qrest_data::tools::tdms;
    const auto sample_path = project_path(
        "resource/wuhan_tdms/S01_20241205_030000_000_SIT_XY001.tdms");

    tdms::LoadOptions load_options;
    load_options.require_sensitivity = false;
    const auto dataset = tdms::load_dataset(sample_path.string(), load_options);

    require_equal(dataset.name,
                  std::string{"S01_20241205_030000_000_SIT_XY001"},
                  "TDMS dataset name");
    require_near(dataset.sample_rate_hz, 200.0, 1e-8, "TDMS sample rate");
    require_equal(
        dataset.north_counts.size(), std::size_t{720000}, "TDMS N count");
    require_equal(
        dataset.east_counts.size(), std::size_t{720000}, "TDMS E count");
    require_equal(
        dataset.vertical_counts.size(), std::size_t{720000}, "TDMS Z count");
    require_equal(
        dataset.timestamps.size(), std::size_t{720000}, "TDMS timestamp count");
    require_near(dataset.selected_sensitivity_raw,
                 0.0,
                 1e-6,
                 "TDMS selected sensitivity");
    require_equal(
        dataset.north_counts.front(), std::int32_t{-1}, "TDMS N first");
    require_equal(dataset.east_counts.front(), std::int32_t{2}, "TDMS E first");
    require_equal(
        dataset.vertical_counts.front(), std::int32_t{-1}, "TDMS Z first");
    require_equal(tdms::format_timestamp_utc(dataset.timestamps.front(), 6),
                  std::string{"2024-12-04T19:00:00.000000Z"},
                  "TDMS first timestamp");
    require_equal(tdms::format_timestamp_utc(dataset.timestamps.back(), 6),
                  std::string{"2024-12-04T19:59:59.995000Z"},
                  "TDMS last timestamp");

    qrest_data::tools::TdmsImportOptions import_options;
    import_options.output_counts = true;
    const auto imported = qrest_data::tools::load_tdms_dataset(
        sample_path.string(), import_options);
    require_equal(
        imported.channel_count, std::size_t{3}, "Imported TDMS channels");
    require_equal(
        imported.sample_count, std::size_t{720000}, "Imported TDMS samples");
    require_near(imported.channel_sequential_data.front(),
                 -1.0,
                 1e-12,
                 "Imported TDMS first value");

    assert_round_trip_qrest(imported,
                            make_metadata({"N", "E", "Z"}, 720000, 0.005),
                            "qrest_data_tools_tdms_import_test.qrest");
}

void test_tdms_directory_mapping() {
    std::vector<std::string> labels;
    labels.reserve(27);
    for (const char direction : {'X', 'Y', 'Z'}) {
        for (int index = 1; index <= 9; ++index) {
            labels.push_back(std::string(1, direction) + std::to_string(index));
        }
    }

    qrest_data::tools::TdmsImportOptions options;
    options.output_counts = true;
    const auto imported = qrest_data::tools::load_tdms_collection(
        project_path("resource/wuhan_tdms").string(),
        make_metadata(labels, 720000, 0.005),
        options);

    require_equal(
        imported.channel_count, std::size_t{27}, "TDMS directory channels");
    require_equal(
        imported.sample_count, std::size_t{720000}, "TDMS directory samples");
    require_near(
        imported.sample_rate_hz, 200.0, 1e-8, "TDMS directory sample rate");
    require_equal(imported.channel_labels.front(),
                  std::string{"X1"},
                  "TDMS directory first label");
    require_equal(imported.channel_labels.back(),
                  std::string{"Z9"},
                  "TDMS directory last label");
    require_near(imported.channel_sequential_data[0],
                 -1.0,
                 1e-12,
                 "TDMS directory maps first source channel to X1");
    require_equal(imported.channel_labels[9],
                  std::string{"Y1"},
                  "TDMS directory sequential mapping reaches Y1");
    require_equal(imported.channel_labels[18],
                  std::string{"Z1"},
                  "TDMS directory sequential mapping reaches Z1");
    qrest_data::tools::require_external_dataset_compatibility(
        imported, make_metadata(labels, 720000, 0.005));
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

void test_unknown_channel_id_validation() {
    auto metadata = make_metadata({"UNKNOWN", "UNKNOWN", "A101"}, 4, 0.01);
    const auto report = qrest_data::tools::validate_metadata(metadata);
    if (!report.ok()) {
        throw std::runtime_error("UNKNOWN ChannelID must not be duplicate");
    }
    require_equal(report.warnings.size(),
                  std::size_t{1},
                  "UNKNOWN ChannelID summary warning");
}

void test_metadata_device_type_and_extension_round_trip() {
    auto metadata = make_metadata({"UNKNOWN"}, 4, 0.01);
    metadata.Extension["SiteTag"] = "Bridge-A";
    metadata.InstrumentInfo.Channels[0].Extension["VendorField"] = 42;

    const auto parsed = qrest_data::Metadata::from_bytes(metadata.to_bytes());
    require_equal(parsed.InstrumentInfo.Channels[0].DeviceType,
                  std::string{"S05"},
                  "DeviceType round-trip");
    require_equal(parsed.Extension.at("SiteTag").get<std::string>(),
                  std::string{"Bridge-A"},
                  "Top-level extension round-trip");
    require_equal(parsed.InstrumentInfo.Channels[0]
                      .Extension.at("VendorField")
                      .get<int>(),
                  42,
                  "Channel extension round-trip");
}

void test_metadata_dt_zero_parses_without_invalid_frequency() {
    auto metadata = make_metadata({"A101"}, 4, 0.01);
    nlohmann::json json = metadata;
    json["DataInfo"]["DT"] = 0.0;

    const auto parsed = qrest_data::Metadata::from_bytes(json.dump());
    require_equal(parsed.DataInfo.DT, 0.0, "DT zero round-trip");
    require_equal(parsed.DataInfo.Frequency,
                  0.0,
                  "Invalid DT should produce zero Frequency");
}

void test_external_channel_mapping_decouples_channel_id() {
    qrest_data::tools::ExternalDataset dataset;
    dataset.source_format = "mapping-test";
    dataset.channel_count = 3;
    dataset.sample_count = 2;
    dataset.sample_rate_hz = 100.0;
    dataset.channel_labels = {"N", "E", "Z"};
    dataset.channel_sequential_data = {1.0, 2.0, 3.0, 4.0, 5.0, 6.0};

    auto metadata = make_metadata({"ABC001", "UNKNOWN", "SENSOR03"}, 2, 0.01);
    const auto mapping =
        qrest_data::tools::make_sequential_channel_mapping(dataset, metadata);
    const auto report = qrest_data::tools::validate_external_channel_mapping(
        dataset, metadata, mapping);
    if (!report.ok()) {
        throw std::runtime_error(
            "External channel mapping should accept arbitrary ChannelID");
    }

    const auto mapped = qrest_data::tools::apply_external_channel_mapping(
        dataset, metadata, mapping);
    require_equal(mapped.channel_labels[0],
                  std::string{"ABC001"},
                  "Mapped arbitrary ChannelID");
    require_equal(mapped.channel_labels[1],
                  std::string{"UNKNOWN"},
                  "Mapped UNKNOWN ChannelID");
    require_near(mapped.channel_sequential_data[0],
                 1.0,
                 1e-12,
                 "Mapped first channel first sample");
    require_near(mapped.channel_sequential_data[4],
                 5.0,
                 1e-12,
                 "Mapped third channel first sample");

    const auto duplicate_report =
        qrest_data::tools::validate_external_channel_mapping(
            dataset, metadata, {{0, 0}, {1, 0}, {2, 2}});
    if (duplicate_report.ok()) {
        throw std::runtime_error("Duplicate target mapping must be rejected");
    }

    const auto missing_report =
        qrest_data::tools::validate_external_channel_mapping(
            dataset, metadata, {{0, 0}, {1, 1}});
    if (missing_report.ok()) {
        throw std::runtime_error(
            "Missing source/target mapping must be rejected");
    }
}

void test_final_validation_reports_independent_packet_errors() {
    auto metadata = make_metadata({"A101"}, 4, 0.01);
    metadata.Header = "BAD_HEADER";

    const auto report = qrest_data::tools::validate_qrest_content(
        metadata, 2, 50, 3, 1, 5, {qrest_data::tools::ValidationMode::Final});

    auto has_error = [&report](const std::string &needle) {
        return std::any_of(report.errors.cbegin(),
                           report.errors.cend(),
                           [&needle](const std::string &message) {
                               return message.find(needle) != std::string::npos;
                           });
    };

    if (!has_error("Header must be qREST_DATA")) {
        throw std::runtime_error("Final validation must report metadata error");
    }
    if (!has_error("Packet channel_count")) {
        throw std::runtime_error(
            "Final validation must continue to packet channel count");
    }
    if (!has_error("Packet data_point_count")) {
        throw std::runtime_error(
            "Final validation must continue to packet NPTS");
    }
    if (!has_error("Packet data size mismatch")) {
        throw std::runtime_error(
            "Final validation must continue to packet data size");
    }
}

} // namespace

int main() {
    try {
        test_mseed_sample();
        test_tdms_sample();
        test_tdms_directory_mapping();
        test_hdf5_bridge();
        test_unknown_channel_id_validation();
        test_metadata_device_type_and_extension_round_trip();
        test_metadata_dt_zero_parses_without_invalid_frequency();
        test_external_channel_mapping_decouples_channel_id();
        test_final_validation_reports_independent_packet_errors();
        std::cout << "qREST import format tests passed.\n";
        return 0;
    } catch (const std::exception &e) {
        std::cerr << "Error: " << e.what() << '\n';
        return 1;
    }
}
