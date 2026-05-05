// qREST_Data 文件存储格式中的文件头

#ifndef QREST_DATA_FILE_HEADER_HPP
#define QREST_DATA_FILE_HEADER_HPP

#include <cstdint>
#include <cstring>
#include <stdexcept>
#include <string>

namespace qrest_data
{

// 强制 1 字节内存对齐的包头纯数据结构 (POD)
#pragma pack(push, 1)
struct FileHeaderPOD
{
    char magic[8];          // 0: 文件标识，固定为 "qREST\0\0\0"
    uint32_t metadata_size; // 8: 元数据长度
    uint32_t data_size;     // 12: 数据包长度
}; // 总长: 16 Bytes
#pragma pack(pop)

// 文件头结构体
class FileHeader
{
public:
    // 默认构造函数
    FileHeader() { init_magic(); }

    FileHeader(uint32_t metadata_size, uint32_t data_size)
        : metadata_size_(metadata_size), data_size_(data_size)
    {
        init_magic();
    }

    // --- Getter ---
    // 返回 std::string 方便外部打印或日志记录
    [[nodiscard]] std::string get_magic() const
    {
        return std::string(magic_, 5);
    }
    [[nodiscard]] uint32_t get_metadata_size() const noexcept
    {
        return metadata_size_;
    }
    [[nodiscard]] uint32_t get_data_size() const noexcept { return data_size_; }

    // 检查文件标识是否正确
    [[nodiscard]] bool is_valid() const noexcept
    {
        const char expected_magic[8] = {
            'q', 'R', 'E', 'S', 'T', '\0', '\0', '\0'};
        return std::memcmp(magic_, expected_magic, 8) == 0;
    }

    // --- Setter ---
    void set_metadata_size(uint32_t size) noexcept { metadata_size_ = size; }
    void set_data_size(uint32_t size) noexcept { data_size_ = size; }

    // --- 序列化 ---
    [[nodiscard]] std::string to_bytes() const
    {
        FileHeaderPOD pod{};
        std::memcpy(pod.magic, magic_, 8);
        pod.metadata_size = metadata_size_;
        pod.data_size = data_size_;

        std::string bytes(sizeof(FileHeaderPOD), '\0');
        std::memcpy(bytes.data(), &pod, sizeof(FileHeaderPOD));

        return bytes;
    }

    // --- 反序列化 ---
    [[nodiscard]] static FileHeader from_bytes(const std::string &bytes)
    {
        if (bytes.size() < sizeof(FileHeaderPOD))
        {
            throw std::runtime_error("File header bytes too short");
        }

        FileHeaderPOD pod;
        std::memcpy(&pod, bytes.data(), sizeof(FileHeaderPOD));

        FileHeader header;
        std::memcpy(header.magic_, pod.magic, 8); // 将读取到的字节拷贝进对象
        header.metadata_size_ = pod.metadata_size;
        header.data_size_ = pod.data_size;

        if (!header.is_valid())
        {
            throw std::runtime_error(
                "Invalid qREST file magic number (Signature mismatch)");
        }

        return header;
    }

private:
    // 辅助函数：初始化 magic_ 数组为 "qREST\0\0\0"
    void init_magic() noexcept
    {
        std::memset(magic_, 0, sizeof(magic_)); // 先全部清零
        std::memcpy(magic_, "qREST", 5);        // 再拷入前 5 个有效字符
    }

private:
    char magic_[8];             // 文件标识，固定值字符数组
    uint32_t metadata_size_{0}; // 元数据大小
    uint32_t data_size_{0};     // 数据大小
};

} // namespace qrest_data

#endif // QREST_DATA_FILE_HEADER_HPP