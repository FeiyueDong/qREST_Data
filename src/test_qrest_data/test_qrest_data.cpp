#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>


// 仅包含 C 接口头文件，模拟外部客户端调用环境
#include "qrest_data/qrest_data.h"

/**
 * @brief 辅助函数：从文件读取所有内容到 std::string
 */
std::string read_file_to_string(const std::string &filepath)
{
    std::ifstream ifs(filepath, std::ios::binary);
    if (!ifs.is_open())
        throw std::runtime_error("Cannot open file: " + filepath);
    return std::string((std::istreambuf_iterator<char>(ifs)),
                       std::istreambuf_iterator<char>());
}

/**
 * @brief 辅助函数：读取文本数据并转置为通道主序
 */
std::vector<double>
read_txt_data(const std::string &filename, int channel_num, int npts)
{
    std::ifstream file(filename);
    if (!file.is_open())
        throw std::runtime_error("Cannot open data file: " + filename);

    std::vector<std::vector<double>> temp_data(
        npts, std::vector<double>(channel_num));
    std::string line;
    int row = 0;
    while (std::getline(file, line) && row < npts)
    {
        std::stringstream ss(line);
        for (int col = 0; col < channel_num; ++col)
        {
            ss >> temp_data[row][col];
        }
        row++;
    }

    // 转置为通道主序
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
    // 为了不依赖外部 JSON 解析库提取通道数，测试时直接作为参数传入
    if (argc < 5)
    {
        std::cout << "Usage: " << argv[0]
                  << " <meta.json> <data.txt> <channel_num> <npts>"
                  << std::endl;
        return 1;
    }

    std::string json_file = argv[1];
    std::string txt_file = argv[2];
    int channel_num = std::stoi(argv[3]);
    int npts = std::stoi(argv[4]);
    std::string qrest_file = "c_api_test_output.qrest";

    try
    {
        std::cout << "[1] Test: qrest_to_bytes" << std::endl;

        // 1. 准备输入数据
        std::string json_str = read_file_to_string(json_file);
        std::vector<double> wave_data =
            read_txt_data(txt_file, channel_num, npts);

        // 2. 构造 C 接口参数结构体
        qrest_c_string_t c_json_input = {const_cast<char *>(json_str.data()),
                                         json_str.size()};
        qrest_c_double_array_t c_data_input = {wave_data.data(),
                                               wave_data.size()};
        qrest_c_byte_stream_t out_stream = {nullptr, 0};

        // 3. 调用 C-API 构建二进制流
        std::cout << "Calling qrest_to_bytes..." << std::endl;
        int build_ret = qrest_to_bytes(
            c_json_input, c_data_input, 1, 0 /* Float32 编码 */, &out_stream);

        if (build_ret != 0)
        {
            throw std::runtime_error("C-API Packaging failed, error code: "
                                     + std::to_string(build_ret));
        }

        std::cout << "Packaging successful! Generated binary stream size: "
                  << out_stream.len << " bytes" << std::endl;

        // 4. 将字节流写入文件
        std::ofstream ofs(qrest_file, std::ios::binary);
        ofs.write(reinterpret_cast<const char *>(out_stream.bytes),
                  out_stream.len);
        ofs.close();
        std::cout << "Written to local file: " << qrest_file << std::endl;

        // 释放 C 层分配的字节流内存
        qrest_free_byte_stream(&out_stream);


        std::cout << "[2] Test: qrest_from_bytes" << std::endl;

        // 1. 读取刚才生成的 qREST 文件作为输入
        std::string bin_data = read_file_to_string(qrest_file);
        qrest_c_byte_stream_t in_stream = {
            reinterpret_cast<uint8_t *>(const_cast<char *>(bin_data.data())),
            bin_data.size()};

        // 2. 初始化用于接收数据的 C 结构体
        qrest_c_data_t *parsed_data = qrest_init_data();

        // 3. 调用 C-API 进行解析
        std::cout << "Calling qrest_from_bytes..." << std::endl;
        int parse_ret = qrest_from_bytes(in_stream, parsed_data);

        if (parse_ret != 0)
        {
            throw std::runtime_error(
                "C-API deserialization failed, error code: "
                + std::to_string(parse_ret));
        }

        // 4. 打印验证解析出来的内容
        std::string magic_str(parsed_data->file_header.magic, 5);
        std::cout << "[Success]" << std::endl;
        std::cout << " -> File header Magic: " << magic_str << std::endl;
        std::cout << " -> Extracted JSON length: "
                  << parsed_data->metadata_json.len << " bytes" << std::endl;
        std::cout << " -> Underlying waveform compression encoding: "
                  << parsed_data->packet_header.data_encodings << std::endl;
        std::cout << " -> Waveform data total: " << parsed_data->packet_data.len
                  << " data points" << std::endl;

        // 核对第一通道前 3 个点
        if (parsed_data->packet_data.len > 3)
        {
            std::cout << " -> Channel 1 first three sample points "
                         "reconstruction result: "
                      << std::fixed << std::setprecision(8)
                      << parsed_data->packet_data.data[0] << ", "
                      << parsed_data->packet_data.data[1] << ", "
                      << parsed_data->packet_data.data[2] << std::endl;
        }

        // 释放 C 层分配的结构体及其内部的动态数组、JSON 字符串
        qrest_free_data(parsed_data);

        std::cout << "\n>>> Testing completed successfully!" << std::endl;
    }
    catch (const std::exception &e)
    {
        std::cerr << "\n[Error]: " << e.what() << std::endl;
        return -1;
    }

    return 0;
}