// qREST_Data数据格式的C接口

#ifndef QREST_DATA_C_API_H
#define QREST_DATA_C_API_H

#include <stddef.h>
#include <stdint.h>

#ifdef _WIN32
#define EXPORT __declspec(dllexport)
#else
#define EXPORT __attribute__((visibility("default")))
#endif

#ifdef __cplusplus
extern "C"
{
#endif

    // --- 1. 数据结构定义 ---

    // 序列化字节流
    typedef struct
    {
        uint8_t *bytes; // 字节流
        size_t len;   // 字节流总长度
    } qrest_c_byte_stream_t;

    // 释放由 qrest_to_bytes 内部申请的二进制流内存
    EXPORT void qrest_free_byte_stream(qrest_c_byte_stream_t *stream);

    // 字符串
    typedef struct
    {
        char *str;  // 字符串内容
        size_t len; // 字符串长度（不包含 \0）
    } qrest_c_string_t;

    // 释放字符串内存
    EXPORT void qrest_free_string(qrest_c_string_t *str);

    // double 数组
    typedef struct
    {
        double *data; // double 数组
        size_t len;   // 数组长度
    } qrest_c_double_array_t;

    // 释放 double 数组内存
    EXPORT void qrest_free_double_array(qrest_c_double_array_t *array);

    // 文件头
    typedef struct
    {
        char magic[8];
        uint32_t metadata_size;
        uint32_t data_size;
    } qrest_c_file_header_t;

    // 数据包头
    typedef struct
    {
        uint8_t magic[2];
        uint16_t source_id;
        uint8_t version;
        uint8_t packet_type;
        uint16_t channel_count;
        uint16_t data_encodings;
        uint16_t sampling_rate;
        uint32_t data_point_count;
        uint64_t timestamp;
        uint32_t body_size;
        uint32_t checksum;
    } qrest_c_packet_header_t;

    // 反序列化输出载体
    typedef struct
    {
        qrest_c_file_header_t file_header;     // 文件头信息
        qrest_c_string_t metadata_json;        // 元数据 JSON 字符串
        qrest_c_packet_header_t packet_header; // 数据包头信息
        qrest_c_double_array_t packet_data;    // 数据包体的 double 数组
    } qrest_c_data_t;

    // 初始化实例
    EXPORT qrest_c_data_t *qrest_init_data();

    // 释放数据结构内存
    EXPORT void qrest_free_data(qrest_c_data_t *data);

    // --- 2. 导出函数 ---

    // @brief 反序列化：从输入的二进制字节流中解析出所有数据对象
    // @param input 输入的二进制字节流
    // @param out_data 输出的解析结果
    // @return 0 成功，-1 参数错误，-2 长度异常，-3 格式或校验失败
    EXPORT int qrest_from_bytes(qrest_c_byte_stream_t input,
                                qrest_c_data_t *out_data);

    // @brief 序列化：从输入的 JSON 字符串和数据包信息构建完整的二进制字节流
    // @param json_str 输入的元数据 JSON 字符串（无需包含尾随\0）
    // @param packet_data 输入的波形数据数组
    // @param source_id 数据源物理ID
    // @param data_encodings 数据编码方式 (0=Float32, 1=Float64, 等)
    // @param out_stream 输出的二进制字节流 (由底层分配，需外部释放)
    // @return 0 成功，-1 参数错误，-2 数据维度不匹配，-3 序列化失败
    EXPORT int qrest_to_bytes(qrest_c_string_t json_str,
                              qrest_c_double_array_t packet_data,
                              uint16_t source_id,
                              uint16_t data_encodings,
                              qrest_c_byte_stream_t *out_stream);

#ifdef __cplusplus
}
#endif

#endif // QREST_DATA_C_API_H
