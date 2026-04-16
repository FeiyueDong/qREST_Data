#include "qrest_data.h"

#include <chrono>
#include <cstring>
#include <span>
#include <string>
#include <vector>

// 引用底层 C++ 头文件
#include "data_struct/data_packet.hpp"
#include "data_struct/file_header.hpp"
#include "data_struct/metadata.hpp"

using namespace qrest_data;

extern "C"
{
    void qrest_free_byte_stream(qrest_c_byte_stream_t *stream)
    {
        if (stream && stream->bytes)
        {
            delete[] stream->bytes;
            stream->bytes = nullptr;
            stream->len = 0;
        }
    }

    void qrest_free_string(qrest_c_string_t *str)
    {
        if (str && str->str)
        {
            delete[] str->str;
            str->str = nullptr;
            str->len = 0;
        }
    }

    void qrest_free_double_array(qrest_c_double_array_t *array)
    {
        if (array && array->data)
        {
            delete[] array->data;
            array->data = nullptr;
            array->len = 0;
        }
    }

    qrest_c_data_t *qrest_init_data()
    {
        qrest_c_data_t *data = new qrest_c_data_t();
        std::memset(data, 0, sizeof(qrest_c_data_t)); // 确保全面归零
        return data;
    }

    void qrest_free_data(qrest_c_data_t *data)
    {
        if (data)
        {
            qrest_free_string(&data->metadata_json);
            qrest_free_double_array(&data->packet_data);
            delete data;
        }
    }

    int qrest_from_bytes(qrest_c_byte_stream_t input, qrest_c_data_t *out_data)
    {
        if (!input.bytes || input.len < 16 || !out_data)
        {
            return -1; // 参数错误或数据流太短
        }

        try
        {
            // 1. 解析 FileHeader
            std::string header_bytes(
                reinterpret_cast<const char *>(input.bytes), 16);
            FileHeader header = FileHeader::from_bytes(header_bytes);

            // 校验总体长度
            size_t expected_total =
                16 + header.get_metadata_size() + header.get_data_size();
            if (input.len != expected_total)
            {
                return -2; // 数据包实际长度与头部声明不符
            }

            // 2. 解析 Metadata JSON
            const char *meta_start =
                reinterpret_cast<const char *>(input.bytes) + 16;
            std::string meta_bytes(meta_start, header.get_metadata_size());

            // 3. 解析 DataPacket
            const char *packet_start = meta_start + header.get_metadata_size();
            std::string packet_raw_bytes(packet_start, header.get_data_size());
            DataPacket packet = DataPacket::from_bytes(packet_raw_bytes);

            // 4. 将 C++ 对象的数据搬运到 C 结构体中 (返回给外部)
            // 4.1 FileHeader
            std::memcpy(
                out_data->file_header.magic, header.get_magic().c_str(), 8);
            out_data->file_header.metadata_size = header.get_metadata_size();
            out_data->file_header.data_size = header.get_data_size();

            // 4.2 Metadata JSON (分配内存，末尾补 \0 方便外部当字符串用)
            out_data->metadata_json.len = meta_bytes.size();
            out_data->metadata_json.str = new char[meta_bytes.size() + 1];
            std::memcpy(out_data->metadata_json.str,
                        meta_bytes.c_str(),
                        meta_bytes.size());
            out_data->metadata_json.str[meta_bytes.size()] = '\0';

            // 4.3 PacketHeader (基础属性映射)
            out_data->packet_header.magic[0] = 0x71;
            out_data->packet_header.magic[1] = 0x44;
            out_data->packet_header.source_id = packet.get_source_id();
            out_data->packet_header.version = packet.get_version();
            out_data->packet_header.packet_type = 0x01;
            out_data->packet_header.channel_count = packet.get_channel_count();
            out_data->packet_header.data_encodings =
                packet.get_data_encodings();
            out_data->packet_header.sampling_rate = packet.get_sampling_rate();
            out_data->packet_header.data_point_count =
                packet.get_data_point_count();
            out_data->packet_header.timestamp = packet.get_timestamp();
            out_data->packet_header.body_size =
                packet.get_packet_size() - 32; // 包体 = 总长 - 包头32字节

            // 4.4 DataPacket Payload (复制底层波形数据)
            const std::vector<double> &wave_data = packet.get_data();
            out_data->packet_data.len = wave_data.size();
            out_data->packet_data.data = new double[wave_data.size()];
            std::memcpy(out_data->packet_data.data,
                        wave_data.data(),
                        wave_data.size() * sizeof(double));

            return 0; // 成功
        }
        catch (const std::exception &e)
        {
            return -3; // 格式错误或 CRC 校验失败
        }
    }

    int qrest_to_bytes(qrest_c_string_t json_str,
                       qrest_c_double_array_t packet_data,
                       uint16_t source_id,
                       uint16_t data_encodings,
                       qrest_c_byte_stream_t *out_stream)
    {

        if (!json_str.str || !packet_data.data || !out_stream)
        {
            return -1; // 参数为空
        }

        try
        {
            // 1. 读取并解析前端语言传入的 JSON，自动提取核心维度参数
            std::string json_std(json_str.str, json_str.len);
            Metadata meta = Metadata::from_bytes(json_std);

            uint16_t channel_count =
                static_cast<uint16_t>(meta.InstrumentInfo.ChannelNum);
            uint32_t data_point_count =
                static_cast<uint32_t>(meta.DataInfo.NPTS);

            // 防止除零错误，计算采样率
            uint16_t sampling_rate =
                (meta.DataInfo.DT > 0.0)
                    ? static_cast<uint16_t>(1.0 / meta.DataInfo.DT)
                    : 100;

            // 解析时间
            std::chrono::sys_time<std::chrono::milliseconds> start_time;
            std::istringstream ss(meta.DataInfo.StartTime);
            ss >> std::chrono::parse("%Y-%m-%dT%H:%M:%S%Ez", start_time);
            auto timestamp = start_time.time_since_epoch().count();

            // 严格核对传入的数组长度是否与 JSON 中定义的矩阵尺寸一致
            if (packet_data.len != (size_t)channel_count * data_point_count)
            {
                return -2; // 维度不匹配
            }

            // 2. 封装波形为 DataPacket
            std::span<const double> data_span(packet_data.data,
                                              packet_data.len);
            DataPacket packet(source_id,
                              channel_count,
                              data_encodings,
                              sampling_rate,
                              data_point_count,
                              timestamp,
                              data_span);

            // 获取底层序列化后的字节
            std::string packet_bytes = packet.to_bytes();
            std::string meta_bytes = meta.to_bytes();

            // 3. 构造 FileHeader
            FileHeader header(meta_bytes.size(), packet_bytes.size());
            std::string header_bytes = header.to_bytes();

            // 4. 将各部分拼接为最终交付的 C 语言二进制流
            size_t total_size =
                header_bytes.size() + meta_bytes.size() + packet_bytes.size();

            out_stream->len = total_size;
            out_stream->bytes = new uint8_t[total_size]; // 为输出分配内存

            // 按序拷贝: Header -> JSON -> Packet
            uint8_t *write_ptr = out_stream->bytes;

            std::memcpy(write_ptr, header_bytes.data(), header_bytes.size());
            write_ptr += header_bytes.size();

            std::memcpy(write_ptr, meta_bytes.data(), meta_bytes.size());
            write_ptr += meta_bytes.size();

            std::memcpy(write_ptr, packet_bytes.data(), packet_bytes.size());

            return 0; // 成功
        }
        catch (const std::exception &e)
        {
            return -3; // JSON 解析失败或其他底层错误
        }
    }

} // extern "C"
