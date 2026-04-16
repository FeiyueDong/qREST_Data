#include <chrono>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>


// 包含之前定义的头文件
#include "data_struct/data_packet.hpp"
#include "data_struct/file_header.hpp"
#include "data_struct/metadata.hpp"


using namespace qrest_data;

/**
 * @brief 读取文本数据文件
 * 格式假定：每一行是一个时间点，每一列是一个通道（Multiplexed）
 * 输出：转置后的数据，即 [通道1所有点, 通道2所有点, ...] (Channel-Sequential)
 */
std::vector<double>
read_txt_data(const std::string &filename, int channel_num, int npts)
{
    std::ifstream file(filename);
    if (!file.is_open())
    {
        throw std::runtime_error("Cannot open data file: " + filename);
    }

    // 预分配空间：先按 [时间点][通道] 临时存储
    std::vector<std::vector<double>> temp_data(
        npts, std::vector<double>(channel_num));

    std::string line;
    int row = 0;
    while (std::getline(file, line) && row < npts)
    {
        std::stringstream ss(line);
        for (int col = 0; col < channel_num; ++col)
        {
            if (!(ss >> temp_data[row][col]))
            {
                throw std::runtime_error("Data format error at line "
                                         + std::to_string(row + 1)
                                         + " Column count insufficient");
            }
        }
        row++;
    }

    if (row < npts)
    {
        std::cerr << "Warning: Actual row count (" << row
                  << ") is less than NPTS defined in metadata (" << npts << ")"
                  << std::endl;
    }

    // 转置为 Channel-Sequential 存储格式
    std::vector<double> flat_data;
    flat_data.reserve(channel_num * npts);
    for (int col = 0; col < channel_num; ++col)
    {
        for (int r = 0; r < npts; ++r)
        {
            flat_data.push_back(temp_data[r][col]);
        }
    }

    return flat_data;
}

int main(int argc, char *argv[])
{
    if (argc < 4)
    {
        std::cout << "Usage: " << argv[0]
                  << " <metadata.json> <data.txt> <output.qrest>" << std::endl;
        return 1;
    }

    std::string meta_file = argv[1];
    std::string data_file = argv[2];
    std::string out_file = argv[3];

    try
    {
        // 1. 读取并解析元数据 JSON
        std::ifstream ifs(meta_file);
        if (!ifs.is_open())
            throw std::runtime_error("Cannot open metadata file: " + meta_file);
        std::string json_content((std::istreambuf_iterator<char>(ifs)),
                                 std::istreambuf_iterator<char>());

        Metadata meta;
        meta = Metadata::from_bytes(json_content);
        std::cout << "Successfully parsed metadata: "
                  << meta.BuildingInfo.ProjectName << std::endl;

        // 获取采样参数
        int channel_num = meta.InstrumentInfo.ChannelNum;
        int npts = meta.DataInfo.NPTS;
        uint16_t sampling_rate = static_cast<uint16_t>(1.0 / meta.DataInfo.DT);

        // 解析时间
        std::cout << "Start Time: " << meta.DataInfo.StartTime << std::endl;
        std::chrono::sys_time<std::chrono::milliseconds> start_time;
        std::istringstream ss(meta.DataInfo.StartTime);
        ss >> std::chrono::parse("%Y-%m-%dT%H:%M:%S%Ez", start_time);
        auto timestamp = start_time.time_since_epoch().count();

        // 2. 读取并转置文本数据
        std::cout << "Reading and transposing text data (Channels: "
                  << channel_num << ", Points: " << npts << ")..." << std::endl;
        std::vector<double> flat_data =
            read_txt_data(data_file, channel_num, npts);

        // 3. 构建数据包 (DataPacket)
        // 此处 DataEncodings 使用 0 (Float32)以平衡精度与大小
        uint16_t encoding = 0;
        DataPacket packet(1, // SourceID
                          channel_num,
                          encoding,
                          sampling_rate,
                          npts,
                          timestamp,
                          flat_data);
        std::string packet_bytes = packet.to_bytes();
        std::cout << "Data packet assembled successfully, size: "
                  << packet_bytes.size() << " bytes" << std::endl;

        // 4. 构建元数据字节流
        std::string meta_bytes = meta.to_bytes();

        // 5. 构建并写入文件头
        FileHeader header(static_cast<uint32_t>(meta_bytes.size()),
                          static_cast<uint32_t>(packet_bytes.size()));

        // 6. 顺序写入二进制文件
        std::ofstream ofs(out_file, std::ios::binary);
        if (!ofs.is_open())
            throw std::runtime_error("Cannot create output file: " + out_file);

        ofs << header.to_bytes(); // 写入 16 字节文件头
        ofs << meta_bytes;        // 写入元数据 JSON
        ofs << packet_bytes;      // 写入数据包 (包含包头和包体)

        ofs.close();
        std::cout << "Successfully generated binary file: " << out_file
                  << std::endl;
        std::cout << "Metadata length: " << header.get_metadata_size()
                  << " bytes" << std::endl;
        std::cout << "Data part length: " << header.get_data_size() << " bytes"
                  << std::endl;
    }
    catch (const std::exception &e)
    {
        std::cerr << "Error: " << e.what() << std::endl;
        return -1;
    }

    return 0;
}