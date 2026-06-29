#include <cmath>
#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>

#include "hdf5_reader.hpp"
#include "hdf5_writer.hpp"
#include "metadata.hpp"

using namespace qrest_data;

int main()
{
    try
    {
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
        for (std::size_t c = 0; c < channel_num; ++c)
        {
            for (std::size_t r = 0; r < npts; ++r)
            {
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

            if (read_npts != npts)
            {
                std::cerr << "  FAIL: NPTS mismatch (" << read_npts << " vs "
                          << npts << ")" << std::endl;
                pass = false;
            }
            if (read_ch != channel_num)
            {
                std::cerr << "  FAIL: Channel count mismatch (" << read_ch
                          << " vs " << channel_num << ")" << std::endl;
                pass = false;
            }
            if (read_meta.BuildingInfo.ProjectName != "TestProject")
            {
                std::cerr << "  FAIL: ProjectName mismatch" << std::endl;
                pass = false;
            }
            if (read_meta.DataInfo.NPTS != 100)
            {
                std::cerr << "  FAIL: NPTS in metadata mismatch" << std::endl;
                pass = false;
            }
            if (read_meta.DataInfo.DT != 0.01)
            {
                std::cerr << "  FAIL: DT in metadata mismatch" << std::endl;
                pass = false;
            }
            if (read_meta.InstrumentInfo.ChannelNum != 4)
            {
                std::cerr << "  FAIL: ChannelNum in metadata mismatch"
                          << std::endl;
                pass = false;
            }
            if (read_meta.InstrumentInfo.Channels.size() != 4)
            {
                std::cerr << "  FAIL: Channels vector size mismatch"
                          << std::endl;
                pass = false;
            }
            if (read_meta.InstrumentInfo.Channels[0].ChannelNo != 1)
            {
                std::cerr << "  FAIL: ChannelNo mismatch" << std::endl;
                pass = false;
            }

            if (read_data.size() != data.size())
            {
                std::cerr << "  FAIL: Data size mismatch (" << read_data.size()
                          << " vs " << data.size() << ")" << std::endl;
                pass = false;
            }

            // Compare all data points
            double max_err = 0.0;
            for (std::size_t i = 0; i < data.size(); ++i)
            {
                double err = std::abs(read_data[i] - data[i]);
                if (err > max_err)
                    max_err = err;
                if (err > 1e-12)
                {
                    pass = false;
                    std::cerr << "  FAIL: Data mismatch at index " << i
                              << " (expected " << data[i] << ", got "
                              << read_data[i] << ")" << std::endl;
                    break;
                }
            }

            if (pass)
            {
                std::cout << "[Verify] PASS - All checks passed!" << std::endl;
                std::cout << "[Verify] Max error: " << max_err << std::endl;
            }
            else
            {
                std::cerr << "[Verify] FAILED!" << std::endl;
                return 1;
            }
        }

        std::cout << "\n========================================" << std::endl;
        std::cout << "  All tests completed successfully!" << std::endl;
        std::cout << "========================================" << std::endl;
    }
    catch (const std::exception &e)
    {
        std::cerr << "\n[Error] " << e.what() << std::endl;
        return 1;
    }

    return 0;
}
