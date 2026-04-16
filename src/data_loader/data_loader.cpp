#include <fstream>
#include <iomanip>
#include <iostream>
#include <string>
#include <vector>

#include "data_struct/data_packet.hpp"
#include "data_struct/file_header.hpp"
#include "data_struct/metadata.hpp"


using namespace qrest_data;

int main(int argc, char *argv[])
{
    if (argc < 4)
    {
        std::cout << "Usage: " << argv[0]
                  << " <input.qrest> <data.txt> <metadata.json>" << std::endl;
        return 1;
    }

    std::string input_file = argv[1];
    std::string output_txt = argv[2];
    std::string output_json = argv[3];

    try
    {
        // 1. 读取二进制文件
        std::ifstream ifs(input_file, std::ios::binary);
        if (!ifs.is_open())
            throw std::runtime_error("Cannot open input file: " + input_file);

        std::cout << ">>> Reading file: " << input_file << std::endl;

        // 解析文件头
        std::string header_bytes(16, '\0');
        ifs.read(&header_bytes[0], 16);
        FileHeader header = FileHeader::from_bytes(header_bytes);

        // 解析元数据
        std::string meta_bytes(header.get_metadata_size(), '\0');
        ifs.read(&meta_bytes[0], header.get_metadata_size());
        Metadata meta = Metadata::from_bytes(meta_bytes);

        int channel_num = meta.InstrumentInfo.ChannelNum;
        int npts = meta.DataInfo.NPTS;

        // 解析数据包
        std::string packet_raw_bytes(header.get_data_size(), '\0');
        ifs.read(&packet_raw_bytes[0], header.get_data_size());
        DataPacket packet = DataPacket::from_bytes(packet_raw_bytes);

        std::cout << "[Success] Channel count: " << channel_num
                  << ", Sample points: " << npts << std::endl;

        // 2. 导出元数据为 JSON 文件
        std::ofstream ofs_json(output_json);
        if (!ofs_json.is_open())
            throw std::runtime_error("Cannot create JSON output file: "
                                     + output_json);
        ofs_json << meta.to_bytes();
        ofs_json.close();
        std::cout << ">>> Exporting metadata to: " << output_json << std::endl;

        // 3. 导出数据为 TXT 文件
        const std::vector<double> &wave_data = packet.get_data();

        std::ofstream ofs(output_txt);
        if (!ofs.is_open())
            throw std::runtime_error("Cannot create TXT output file: "
                                     + output_txt);

        std::cout << ">>> Exporting data to: " << output_txt << " ..."
                  << std::endl;

        // 写入数据：转置回时间主序 (按行写入)
        for (int r = 0; r < npts; ++r)
        {
            // ofs << r; // 第一列为时间步索引
            for (int c = 0; c < channel_num; ++c)
            {
                // 计算该数据点在通道主序数组中的绝对一维索引
                size_t index = static_cast<size_t>(c) * npts + r;
                ofs << " " << std::fixed << std::setprecision(8)
                    << wave_data[index];
            }
            ofs << "\n";
        }

        ofs.close();
        std::cout << ">>> Exporting complete! Total " << npts
                  << " rows of data written." << std::endl;
    }
    catch (const std::exception &e)
    {
        std::cerr << "\n[Error]: " << e.what() << std::endl;
        return -1;
    }

    return 0;
}