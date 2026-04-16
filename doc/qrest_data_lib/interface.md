# qREST_Data接口文档

**接口版本**: v1.0.1
**最后更新**: 2026-04-16

以纯C语言接口的形式提供了一个动态库，用于解析和生成符合qREST数据协议的字节流。该接口定义了数据包的结构、元数据的格式以及相关的读写函数，方便其他编程语言调用和集成。

## 1. 数据结构

### 1.1 基本数据结构

#### 字节流 (`qrest_c_byte_stream_t`)

表示一个字节流，包含一个指向数据的指针和数据长度。包含以下成员：

- `bytes`: 指向字节流数据的指针。
- `len`: 字节流数据的长度。

拥有销毁函数 `qrest_free_byte_stream()`，用于释放字节流占用的内存。

#### 字符串 (`qrest_c_string_t`)

表示一个字符串，包含一个指向数据的指针和数据长度。包含以下成员：

- `str`: 指向字符串数据的指针。
- `len`: 字符串数据的长度。

拥有销毁函数 `qrest_free_string()`，用于释放字符串占用的内存。

#### double 数组 (`qrest_c_double_array_t`)

表示一个 double 数组，包含一个指向数据的指针和数据长度。包含以下成员：

- `data`: 指向 double 数组数据的指针。
- `len`: double 数组数据的长度。

拥有销毁函数 `qrest_free_double_array()`，用于释放 double 数组占用的内存。

### 1.2 qREST_Data 数据结构

#### 文件头 (`qrest_c_file_header_t`)

表示一个文件头，包含魔数、元数据大小和数据大小，各项定义参考数据存储规范中的文件头词条。该结构体包含以下成员：

- `magic`: 魔数，用于标识文件格式。
- `metadata_size`: 元数据的大小。
- `data_size`: 数据的大小。

> 注：结构体成员简单，不会自主分配内存，故未定义销毁函数。调用者负责管理内存。

#### 数据包头 (`qrest_c_packet_header_t`)

表示一个数据包头，包含数据源ID、版本号、数据类型等信息，各项定义参考数据存储规范中的数据包头词条。该结构体包含以下成员：

- `magic`: 魔数，用于标识数据包格式。
- `source_id`: 数据源物理ID。
- `version`: 版本号。
- `packet_type`: 数据包类型。
- `channel_count`: 通道数量。
- `data_encodings`: 数据编码方式。
- `sampling_rate`: 采样率。
- `data_point_count`: 数据点数量。
- `timestamp`: 时间戳。
- `body_size`: 包体大小。
- `checksum`: 校验和。

> 注：结构体成员简单，不会自主分配内存，故未定义销毁函数。调用者负责管理内存。

#### 反序列化输出载体 (`qrest_c_data_t`)

表示一个反序列化输出载体，包含文件头、元数据和数据包信息。包含以下成员：

- `file_header`: 文件头信息。
- `metadata_json`: 元数据 JSON 字符串。
- `packet_header`: 数据包头信息。
- `packet_data`: 数据包体的 double 数组。

拥有初始化函数 `qrest_init_data()` 用于创建一个新的数据载体实例指针；拥有销毁函数 `qrest_free_data()` 用于释放数据载体占用的内存。

## 2. 接口函数

### 2.1 反序列化函数 (`qrest_from_bytes`)

从输入的二进制字节流中解析出所有数据对象，并将结果存储在一个 `qrest_c_data_t` 结构体实例中。函数签名如下：

```c
int qrest_from_bytes(qrest_c_byte_stream_t input,
                     qrest_c_data_t *out_data);
```

说明：
- `input`: 输入的二进制字节流。
- `out_data`: 输出参数，指向一个 `qrest_c_data_t` 结构体实例的指针，用于存储解析结果。
- 返回值：函数执行结果，0 表示成功，非0表示失败。错误值定义如下：
  - `-1`: 参数错误或数据流太短。
  - `-2`: 数据包实际长度与头部声明不符。
  - `-3`: 格式错误或 CRC 校验失败。

### 2.2 序列化函数 (`qrest_to_bytes`)

将输入的 `qrest_c_data_t` 结构体实例中的数据对象序列化为符合 qREST 数据协议的二进制字节流。函数签名如下：

```c
int qrest_to_bytes(qrest_c_string_t json_str,
                   qrest_c_double_array_t packet_data,
                   uint16_t source_id,
                   uint16_t data_encodings,
                   qrest_c_byte_stream_t *out_stream);
```

说明：
- `json_str`: 输入的元数据 JSON 字符串。
- `packet_data`: 输入的数据包体的 double 数组。
- `source_id`: 输入的数据源物理ID。
- `data_encodings`: 输入的数据编码方式。
- `out_stream`: 输出参数，指向一个 `qrest_c_byte_stream_t` 结构体实例的指针，用于存储序列化结果。
- 返回值：函数执行结果，0 表示成功，非0表示失败。错误值定义如下：
  - `-1`: 参数为空。
  - `-2`: 维度不匹配。
  - `-3`: JSON 解析失败或其他内部错误。

## 3. 测试用例

为了测试动态库的功能，验证接口正确性，并提供一个使用示例，编写了一个测试用例 `test_qrest_data`，调用了上述接口函数进行数据的序列化和反序列化，并检查结果是否符合预期。

### 3.1 测试程序 (`test_qrest_data`)

编写的测试程序使用方式如下：

```bash
test_qrest_data <meta.json> <data.txt> <channel_num> <npts>
```

其中：
- `<meta.json>`：包含数据元信息的 JSON 文件。
- `<data.txt>`：包含实际数据内容的文本文件。
- `<channel_num>`：数据的通道数量。
- `<npts>`：每个通道的数据点数量。

> 注：这里选择手动输入了通道数量和数据点数量，只是为了方便测试接口的维度检查功能。实际使用中应当从元数据 JSON 中解析出这些信息。

### 库的使用

作为参考，调用者可以按照如下的步骤调用该动态库的功能：

#### 反序列化

1. 程序接收一个字节流放入一段连续内存；
2. 使用 `qrest_init_data()` 创建一个 `qrest_c_data_t` 结构体实例；
3. 调用 `qrest_from_bytes()` 解析字节流，获取 `qrest_c_data_t`，包含文件头、元数据和数据包信息；
4. 使用解析得到的数据进行后续处理。

#### 序列化

1. 准备好元数据 JSON 字符串和数据包体的 double 数组，创建一个空的 `qrest_c_byte_stream_t` 结构体实例；
2. 调用 `qrest_to_bytes()` 将数据对象序列化为字节流；
3. 使用得到的字节流进行后续处理，如写入文件或发送网络请求。