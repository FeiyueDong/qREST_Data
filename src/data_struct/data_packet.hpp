// qREST_Data 文件存储格式中的数据包

#ifndef QREST_DATA_DATA_PACKET_HPP
#define QREST_DATA_DATA_PACKET_HPP

#include <cstdint>
#include <cstring>
#include <span>
#include <stdexcept>
#include <string>
#include <vector>

namespace qrest_data
{

// 规范定义的非反射型 CRC32 算法
// 参数: Poly=0x04C11DB7, Init=0xFFFFFFFF, RefIn=False, RefOut=False,
// XorOut=0xFFFFFFFF
inline uint32_t crc32(const uint8_t *data, size_t length)
{
    uint32_t crc = 0xFFFFFFFF;

    for (size_t i = 0; i < length; i++)
    {
        crc ^= (static_cast<uint32_t>(data[i])
                << 24); // 不做位翻转，移至最高位异或

        for (int j = 0; j < 8; j++)
        {
            if (crc & 0x80000000) // 检查最高位
            {
                crc = (crc << 1) ^ 0x04C11DB7;
            }
            else
            {
                crc <<= 1;
            }
        }
    }

    return crc ^ 0xFFFFFFFF; // 规范要求的 XorOut
}

// 强制 1 字节内存对齐的包头纯数据结构 (POD)
#pragma pack(push, 1)
struct PacketHeaderPOD
{
    uint16_t magic;            // 0: 0x7144 ("qD")
    uint16_t source_id;        // 2: 数据源ID
    uint8_t version;           // 4: 协议版本 0x01
    uint8_t packet_type;       // 5: 数据包类型 0x01
    uint16_t channel_count;    // 6: 通道数量
    uint16_t data_encodings;   // 8: 数据编码方式 (0, 1, 10, 11)
    uint16_t sampling_rate;    // 10: 采样率(Hz)
    uint32_t data_point_count; // 12: 每个通道的数据点数量
    uint64_t timestamp;        // 16: 时间戳（毫秒）
    uint32_t body_size;        // 24: 数据包包体长度
    uint32_t checksum;         // 28: CRC32校验和
}; // 总长: 32 Bytes
#pragma pack(pop)

// 数据包类
class DataPacket
{
public:
    // 默认构造函数
    DataPacket() = default;

    // 核心构造函数：内存中始终以 double 存储波形，序列化时再做类型转换
    DataPacket(uint16_t source_id,
               uint8_t version,
               uint16_t channel_count,
               uint16_t data_encodings,
               uint16_t sampling_rate,
               uint32_t data_point_count,
               uint64_t timestamp,
               std::span<const double> data)
        : source_id_(source_id),
          version_(version),
          channel_count_(channel_count),
          data_encodings_(data_encodings),
          sampling_rate_(sampling_rate),
          data_point_count_(data_point_count),
          timestamp_(timestamp)
    {
        if (data.size()
            != static_cast<size_t>(channel_count) * data_point_count)
        {
            throw std::invalid_argument(
                "Data size does not match channel count and data point count");
        }

        // 验证编码类型是否合法
        get_type_size(data_encodings_);

        data_.assign(data.begin(), data.end());
    }

    // --- Getter 方法 ---
    [[nodiscard]] uint16_t get_source_id() const { return source_id_; }
    [[nodiscard]] uint8_t get_version() const { return version_; }
    [[nodiscard]] uint16_t get_channel_count() const { return channel_count_; }
    [[nodiscard]] uint16_t get_data_encodings() const
    {
        return data_encodings_;
    }
    [[nodiscard]] uint16_t get_sampling_rate() const { return sampling_rate_; }
    [[nodiscard]] uint32_t get_data_point_count() const
    {
        return data_point_count_;
    }
    [[nodiscard]] uint64_t get_timestamp() const { return timestamp_; }

    // 获取当前对象的预期总字节数 (包头 + 包体)
    [[nodiscard]] uint32_t get_packet_size() const
    {
        return sizeof(PacketHeaderPOD)
               + static_cast<uint32_t>(data_.size()
                                       * get_type_size(data_encodings_));
    }

    // 获取数据包体引用 (内存中统一为 double)
    [[nodiscard]] const std::vector<double> &get_data() const { return data_; }

    // --- 序列化 (内存对象 -> 二进制数据流) ---
    [[nodiscard]] std::string to_bytes() const
    {
        size_t type_size = get_type_size(data_encodings_);
        uint32_t expected_body_size =
            static_cast<uint32_t>(data_.size() * type_size);

        // 分配完整数据包所需的连续内存空间
        std::string bytes(sizeof(PacketHeaderPOD) + expected_body_size, '\0');

        // 1. 序列化包体 (动态类型转换)
        uint8_t *body_ptr =
            reinterpret_cast<uint8_t *>(bytes.data() + sizeof(PacketHeaderPOD));
        for (size_t i = 0; i < data_.size(); ++i)
        {
            size_t offset = i * type_size;
            if (data_encodings_ == 0) // Float32
            {
                float val = static_cast<float>(data_[i]);
                std::memcpy(body_ptr + offset, &val, type_size);
            }
            else if (data_encodings_ == 1) // Float64
            {
                double val = data_[i];
                std::memcpy(body_ptr + offset, &val, type_size);
            }
            else if (data_encodings_ == 10) // Int16
            {
                int16_t val = static_cast<int16_t>(data_[i]);
                std::memcpy(body_ptr + offset, &val, type_size);
            }
            else if (data_encodings_ == 11) // Int32
            {
                int32_t val = static_cast<int32_t>(data_[i]);
                std::memcpy(body_ptr + offset, &val, type_size);
            }
        }

        // 2. 计算包体 CRC32
        uint32_t computed_checksum = crc32(body_ptr, expected_body_size);

        // 3. 构建包头
        PacketHeaderPOD header{};
        header.magic = 0x7144;
        header.source_id = source_id_;
        header.version = version_;
        header.packet_type = packet_type_;
        header.channel_count = channel_count_;
        header.data_encodings = data_encodings_;
        header.sampling_rate = sampling_rate_;
        header.data_point_count = data_point_count_;
        header.timestamp = timestamp_;
        header.body_size = expected_body_size;
        header.checksum = computed_checksum;

        // 4. 将包头拷贝到字节流头部
        std::memcpy(bytes.data(), &header, sizeof(PacketHeaderPOD));

        return bytes;
    }

    // --- 反序列化 (二进制数据流 -> 内存对象) ---
    [[nodiscard]] static DataPacket from_bytes(const std::string &bytes)
    {
        if (bytes.size() < sizeof(PacketHeaderPOD))
        {
            throw std::runtime_error(
                "Bytes too short to contain a valid header");
        }

        // 1. 解析包头
        PacketHeaderPOD header;
        std::memcpy(&header, bytes.data(), sizeof(PacketHeaderPOD));

        if (header.magic != 0x7144)
        {
            throw std::runtime_error("Invalid packet magic");
        }
        if (header.packet_type != 0x01)
        {
            throw std::runtime_error("Unsupported packet type");
        }
        if (bytes.size() != sizeof(PacketHeaderPOD) + header.body_size)
        {
            throw std::runtime_error("Packet size mismatch");
        }

        // 2. 校验包体 CRC32
        const uint8_t *body_ptr = reinterpret_cast<const uint8_t *>(
            bytes.data() + sizeof(PacketHeaderPOD));
        uint32_t computed_checksum = crc32(body_ptr, header.body_size);
        if (computed_checksum != header.checksum)
        {
            throw std::runtime_error(
                "Invalid packet checksum (Data corrupted)");
        }

        // 3. 构造对象实例
        DataPacket packet;
        packet.source_id_ = header.source_id;
        packet.version_ = header.version;
        packet.packet_type_ = header.packet_type;
        packet.channel_count_ = header.channel_count;
        packet.data_encodings_ = header.data_encodings;
        packet.sampling_rate_ = header.sampling_rate;
        packet.data_point_count_ = header.data_point_count;
        packet.timestamp_ = header.timestamp;

        // 4. 解析包体 (动态类型还原为 double)
        size_t total_points =
            static_cast<size_t>(header.channel_count) * header.data_point_count;
        size_t type_size = get_type_size(header.data_encodings);

        if (header.body_size != total_points * type_size)
        {
            throw std::runtime_error(
                "Body size does not match encoding scale and point count");
        }

        packet.data_.resize(total_points);

        for (size_t i = 0; i < total_points; ++i)
        {
            size_t offset = i * type_size;
            if (header.data_encodings == 0) // Float32
            {
                float val;
                std::memcpy(&val, body_ptr + offset, type_size);
                packet.data_[i] = static_cast<double>(val);
            }
            else if (header.data_encodings == 1) // Float64
            {
                double val;
                std::memcpy(&val, body_ptr + offset, type_size);
                packet.data_[i] = val;
            }
            else if (header.data_encodings == 10) // Int16
            {
                int16_t val;
                std::memcpy(&val, body_ptr + offset, type_size);
                packet.data_[i] = static_cast<double>(val);
            }
            else if (header.data_encodings == 11) // Int32
            {
                int32_t val;
                std::memcpy(&val, body_ptr + offset, type_size);
                packet.data_[i] = static_cast<double>(val);
            }
        }

        return packet;
    }

private:
    // 辅助函数：根据编码类型返回占用字节数
    static size_t get_type_size(uint16_t encoding)
    {
        switch (encoding)
        {
            case 0:
                return 4; // Float32
            case 1:
                return 8; // Float64
            case 10:
                return 2; // Int16
            case 11:
                return 4; // Int32
            default:
                throw std::invalid_argument(
                    "Unsupported data encoding protocol");
        }
    }

private:
    // 对象内部状态 (剥离了与网络传输直接相关的序列化中间变量如 checksum_ 和
    // body_size_)
    uint16_t source_id_{};
    uint8_t version_{1};
    uint8_t packet_type_{1};
    uint16_t channel_count_{};
    uint16_t data_encodings_{};
    uint16_t sampling_rate_{};
    uint32_t data_point_count_{};
    uint64_t timestamp_{};

    // 数据数组，统一以高精度浮点数常驻内存
    std::vector<double> data_{};
};

} // namespace qrest_data

#endif // QREST_DATA_DATA_PACKET_HPP