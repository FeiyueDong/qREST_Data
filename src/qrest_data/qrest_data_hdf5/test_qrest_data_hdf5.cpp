#include <cmath>
#include <cstdlib>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

#include "hdf5_reader.hpp"
#include "hdf5_writer.hpp"
#include "metadata.hpp"

using namespace qrest_data;

namespace {

void require_true(bool condition, const char *message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

void check_hdf5(herr_t status, const char *message) {
    if (status < 0) {
        throw std::runtime_error(message);
    }
}

void write_mismatched_hdf5_file(const std::string &path,
                                const Metadata &metadata,
                                std::size_t npts,
                                std::size_t channel_num) {
    const hid_t file =
        H5Fcreate(path.c_str(), H5F_ACC_TRUNC, H5P_DEFAULT, H5P_DEFAULT);
    if (file < 0) {
        throw std::runtime_error("failed to create mismatched HDF5 file");
    }

    const std::string meta_str = metadata.to_bytes();
    hsize_t metadata_dims[1] = {static_cast<hsize_t>(meta_str.size())};
    hid_t metadata_space = H5Screate_simple(1, metadata_dims, nullptr);
    hid_t metadata_dataset = H5Dcreate2(file,
                                        "Metadata",
                                        H5T_NATIVE_CHAR,
                                        metadata_space,
                                        H5P_DEFAULT,
                                        H5P_DEFAULT,
                                        H5P_DEFAULT);
    check_hdf5(H5Dwrite(metadata_dataset,
                        H5T_NATIVE_CHAR,
                        H5S_ALL,
                        H5S_ALL,
                        H5P_DEFAULT,
                        meta_str.data()),
               "failed to write mismatched HDF5 metadata");
    H5Dclose(metadata_dataset);
    H5Sclose(metadata_space);

    hid_t scalar = H5Screate(H5S_SCALAR);
    unsigned long long npts_attr = npts;
    hid_t npts_id = H5Acreate2(
        file, "npts", H5T_NATIVE_ULLONG, scalar, H5P_DEFAULT, H5P_DEFAULT);
    check_hdf5(H5Awrite(npts_id, H5T_NATIVE_ULLONG, &npts_attr),
               "failed to write mismatched HDF5 npts attribute");
    H5Aclose(npts_id);

    unsigned long long channel_attr = channel_num;
    hid_t channel_id = H5Acreate2(file,
                                  "channel_num",
                                  H5T_NATIVE_ULLONG,
                                  scalar,
                                  H5P_DEFAULT,
                                  H5P_DEFAULT);
    check_hdf5(H5Awrite(channel_id, H5T_NATIVE_ULLONG, &channel_attr),
               "failed to write mismatched HDF5 channel attribute");
    H5Aclose(channel_id);
    H5Sclose(scalar);

    hsize_t data_dims[2] = {static_cast<hsize_t>(npts - 1u),
                            static_cast<hsize_t>(channel_num)};
    hid_t data_space = H5Screate_simple(2, data_dims, nullptr);
    hid_t data_dataset = H5Dcreate2(file,
                                    "acceleration",
                                    H5T_NATIVE_DOUBLE,
                                    data_space,
                                    H5P_DEFAULT,
                                    H5P_DEFAULT,
                                    H5P_DEFAULT);
    std::vector<double> data((npts - 1u) * channel_num, 0.0);
    check_hdf5(H5Dwrite(data_dataset,
                        H5T_NATIVE_DOUBLE,
                        H5S_ALL,
                        H5S_ALL,
                        H5P_DEFAULT,
                        data.data()),
               "failed to write mismatched HDF5 acceleration data");
    H5Dclose(data_dataset);
    H5Sclose(data_space);
    H5Fclose(file);
}

} // namespace

int main() {
    try {
        // ========== 1. Create test metadata ==========
        std::string json = R"({
            "Header": "qREST_DATA",
            "Version": [1, 0, 0],
            "Units": ["m", "s"],
            "BuildingInfo": {
                "ProjectName": "TestProject",
                "GeoLocation": {
                    "Longitude": 21.85,
                    "Latitude": 95.95,
                    "NorthAngle": 0.0
                },
                "StructuralType": "SteelFrame",
                "StructuralFootprint": {
                    "Shape": "Rectangular",
                    "Parameters": { "Length": 10, "Width": 10 },
                    "BoundingBox": {
                        "MaxX": 5, "MinX": -5,
                        "MaxY": 5, "MinY": -5
                    }
                },
                "ElevationNum": 3,
                "Elevation": [0.0, 5.0, 10.0]
            },
            "InstrumentInfo": {
                "Provider": "TestProvider",
                "ChannelNum": 4,
                "Channels": [
                    {"ChannelNo": 1, "ChannelID": "CH1", "Measurand": "Acceleration", "Scale": 1, "Azimuth": 0.0, "LocationXYZ": [0, 0, 0]},
                    {"ChannelNo": 2, "ChannelID": "CH2", "Measurand": "Acceleration", "Scale": 1, "Azimuth": 90.0, "LocationXYZ": [1, 0, 0]},
                    {"ChannelNo": 3, "ChannelID": "CH3", "Measurand": "Acceleration", "Scale": 1, "Azimuth": 180.0, "LocationXYZ": [0, 1, 0]},
                    {"ChannelNo": 4, "ChannelID": "CH4", "Measurand": "Acceleration", "Scale": 1, "Azimuth": 270.0, "LocationXYZ": [0, 0, 1]}
                ]
            },
            "DataInfo": {
                "EventName": "TestCase",
                "StartTime": "2025-03-28T14:20:00.000+08:00",
                "NPTS": 100,
                "DT": 0.01,
                "Corrected": "NULL"
            }
        })";

        Metadata meta = Metadata::from_bytes(json);
        std::size_t npts = 100;
        std::size_t channel_num = 4;

        std::cout << "========================================" << std::endl;
        std::cout << "  qREST Data HDF5 Module Test" << std::endl;
        std::cout << "========================================" << std::endl;
        std::cout << "Metadata: " << meta.BuildingInfo.ProjectName << std::endl;
        std::cout << "Channels: " << channel_num << ", NPTS: " << npts
                  << std::endl;

        // ========== 2. Create synthetic channel-sequential data ==========
        std::vector<double> data(channel_num * npts);
        for (std::size_t c = 0; c < channel_num; ++c) {
            for (std::size_t r = 0; r < npts; ++r) {
                data[c * npts + r] = static_cast<double>(c * 10000 + r);
            }
        }

        // Reference: first 3 samples of first channel
        std::cout << "Ref[0]: " << data[0] << ", Ref[1]: " << data[1]
                  << ", Ref[2]: " << data[2] << std::endl;

        // ========== 3. Write to HDF5 ==========
        std::cout << "\n[Write] Writing to test_output.h5 ..." << std::endl;
        {
            Hdf5Writer writer;
            writer.open("test_output.h5");
            writer.write(meta, data, npts, channel_num);
            std::cout << "[Write] Done!" << std::endl;
        }

        // ========== 4. Read back from HDF5 ==========
        std::cout << "\n[Read] Reading from test_output.h5 ..." << std::endl;
        {
            Hdf5Reader reader;
            reader.open("test_output.h5");

            std::size_t read_npts = reader.get_npts();
            std::size_t read_ch = reader.get_channel_num();
            std::cout << "[Read] NPTS: " << read_npts
                      << ", Channels: " << read_ch << std::endl;

            Metadata read_meta = reader.read_metadata();
            std::cout << "[Read] Project: "
                      << read_meta.BuildingInfo.ProjectName << std::endl;
            std::cout << "[Read] Event: " << read_meta.DataInfo.EventName
                      << std::endl;
            std::cout << "[Read] DT: " << read_meta.DataInfo.DT << std::endl;
            std::cout << "[Read] StartTime: " << read_meta.DataInfo.StartTime
                      << std::endl;
            std::cout << "[Read] ElevationNum: "
                      << read_meta.BuildingInfo.ElevationNum << std::endl;
            std::cout << "[Read] Provider: "
                      << read_meta.InstrumentInfo.Provider << std::endl;

            std::vector<double> read_data = reader.read_accform();

            // ========== 5. Verify round-trip ==========
            std::cout << "\n[Verify] Checking data integrity ..." << std::endl;
            bool pass = true;

            if (read_npts != npts) {
                std::cerr << "  FAIL: NPTS mismatch (" << read_npts << " vs "
                          << npts << ")" << std::endl;
                pass = false;
            }
            if (read_ch != channel_num) {
                std::cerr << "  FAIL: Channel count mismatch (" << read_ch
                          << " vs " << channel_num << ")" << std::endl;
                pass = false;
            }
            if (read_meta.BuildingInfo.ProjectName != "TestProject") {
                std::cerr << "  FAIL: ProjectName mismatch" << std::endl;
                pass = false;
            }
            if (read_meta.DataInfo.NPTS != 100) {
                std::cerr << "  FAIL: NPTS in metadata mismatch" << std::endl;
                pass = false;
            }
            if (read_meta.DataInfo.DT != 0.01) {
                std::cerr << "  FAIL: DT in metadata mismatch" << std::endl;
                pass = false;
            }
            if (read_meta.InstrumentInfo.ChannelNum != 4) {
                std::cerr << "  FAIL: ChannelNum in metadata mismatch"
                          << std::endl;
                pass = false;
            }
            if (read_meta.InstrumentInfo.Channels.size() != 4) {
                std::cerr << "  FAIL: Channels vector size mismatch"
                          << std::endl;
                pass = false;
            }
            if (read_meta.InstrumentInfo.Channels[0].ChannelNo != 1) {
                std::cerr << "  FAIL: ChannelNo mismatch" << std::endl;
                pass = false;
            }

            if (read_data.size() != data.size()) {
                std::cerr << "  FAIL: Data size mismatch (" << read_data.size()
                          << " vs " << data.size() << ")" << std::endl;
                pass = false;
            }

            // Compare all data points
            double max_err = 0.0;
            for (std::size_t i = 0; i < data.size(); ++i) {
                double err = std::abs(read_data[i] - data[i]);
                if (err > max_err)
                    max_err = err;
                if (err > 1e-12) {
                    pass = false;
                    std::cerr << "  FAIL: Data mismatch at index " << i
                              << " (expected " << data[i] << ", got "
                              << read_data[i] << ")" << std::endl;
                    break;
                }
            }

            if (pass) {
                std::cout << "[Verify] PASS - All checks passed!" << std::endl;
                std::cout << "[Verify] Max error: " << max_err << std::endl;
            } else {
                std::cerr << "[Verify] FAILED!" << std::endl;
                return 1;
            }
        }

        std::cout << "\n[Validate] Checking writer size guard ..." << std::endl;
        {
            bool caught = false;
            try {
                Hdf5Writer writer;
                writer.open("test_bad_size.h5");
                writer.write(meta, std::vector<double>{1.0}, npts, channel_num);
            } catch (const std::exception &e) {
                caught =
                    std::string(e.what()).find("expected") != std::string::npos;
            }
            std::remove("test_bad_size.h5");
            require_true(caught, "writer should reject mismatched data length");
        }

        std::cout << "[Validate] Checking reader dimension guard ..."
                  << std::endl;
        {
            write_mismatched_hdf5_file(
                "test_bad_dims.h5", meta, npts, channel_num);

            bool caught = false;
            try {
                Hdf5Reader reader;
                reader.open("test_bad_dims.h5");
                (void)reader.read_accform();
            } catch (const std::exception &e) {
                caught = std::string(e.what()).find("dimensions")
                         != std::string::npos;
            }
            std::remove("test_bad_dims.h5");
            require_true(caught,
                         "reader should reject mismatched acceleration dims");
        }
        std::remove("test_output.h5");

        std::cout << "\n========================================" << std::endl;
        std::cout << "  All tests completed successfully!" << std::endl;
        std::cout << "========================================" << std::endl;
    } catch (const std::exception &e) {
        std::cerr << "\n[Error] " << e.what() << std::endl;
        return 1;
    }

    return 0;
}
