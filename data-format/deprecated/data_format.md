# qREST数据格式说明

本文档说明qREST模块中使用的数据文件格式规范，包含文本格式和二进制格式两种类型的数据文件。

## 设计方案

- **2025.12.30 dfy**

主要信息：
- 文件头信息：包含版本号、时间戳、测点位置、项目名称等信息。
- 测点信息：包含通道数量、楼层数量、每个通道的详细信息（方向、位置坐标等）。
- 楼层信息：包含每个楼层的标高信息。
- 时程数据：包含每个通道的时程数据点。

字节对齐：4字节对齐

| 字段名      | 类型    | 长度 | 对齐  | 说明                           |
| ----------- | ------- | ---- | ----- | ------------------------------ |
| Header      | char    | 16   | 0-15  | 文件头标识，固定为"qREST_DATA" |
| Version     | uint32  | 2    | 16-23 | 版本号，当前为{1,0}            |
| Time        | uint32  | 1    | 24-27 | 数据采集的起始时间(Unix时间戳) |
| Location    | float32 | 2    | 28-35 | 测点的地理位置信息(经纬度)     |
| Project     | char    | 32   | 36-67 | 项目名称                       |
| NPTS        | uint32  | 1    | 68-71 | 时间点数                       |
| DT          | float32 | 1    | 72-75 | 采样时间间隔                   |
| Unit        | uint32  | 1    | 76-79 | 加速度单位(0:cm/s²,1:m/s²,2:g) |
| ChannelNum  | uint32  | 1    | 80-83 | 通道数量                       |
| FloorNum    | uint32  | 1    | 84-87 | 楼层数量                       |
| Begin       | float32 | 1    | 88-91 | 测点信息起始符                 |
| ChannelInfo | float32 | 待定 | 92-   | 每个通道的详细信息(见下文)     |
| End         | float32 | 1    | ...   | 测点信息结束符                 |
| Begin       | float32 | 1    | ...   | 楼层信息起始符                 |
| FloorInfo   | float32 | 待定 | ...   | 每个楼层的详细信息(见下文)     |
| End         | float32 | 1    | ...   | 楼层信息结束符                 |
| Data        | float32 | 待定 | ...   | 时程数据                       |

### ChannelInfo结构

ChannelInfo部分被ChannelInfoB和ChannelInfoE包围，按照通道顺序存储。每个通道的信息占用4个float32字节，包含以下字段：

| 字段名    | 类型    | 说明              |
| --------- | ------- | ----------------- |
| Direction | float32 | 通道方向(0-360度) |
| LocationX | float32 | 通道X坐标         |
| LocationY | float32 | 通道Y坐标         |
| LocationZ | float32 | 通道Z坐标         |

Direction表示通道的方向：0度为正北，顺时针增加。
LocationX/Y/Z表示通道在空间中的位置坐标。

ChannelInfo部分的长度取决于ChannelNum，总字节长度为ChannelNum * 4 * 4字节。

### FloorInfo结构

FloorInfo部分被FloorInfoB和FloorInfoE包围，按照楼层顺序存储。每个楼层的信息占用1个float32字节，包含字段含义为楼层标高。

FloorInfo部分的长度取决于FloorNum，总字节长度为FloorNum * 4字节。

### Data结构

时程数据按通道顺序存储，每个通道包含通道数据起始符，NPTS个float32数据点，通道数据结束符。数据排列顺序为：
Begin, Channel1_Data[0], Channel1_Data[1], ..., Channel1_Data[NPTS-1], End,
Begin, Channel2_Data[0], Channel2_Data[1], ..., Channel2_Data[NPTS-1], End,
...
Begin, ChannelN_Data[0], ChannelN_Data[1], ..., ChannelN_Data[NPTS-1], End

数据部分的总字节长度为ChannelNum * NPTS * 4字节。

### 说明

1. 数据文件以二进制格式存储，确保高效读取和存储。
2. 字节对齐为4字节，即每个字段的起始位置均为4的倍数。
3. 版本号用于区分不同的数据格式版本，便于后续扩展。
4. 时间采用Unix时间戳格式，表示自1970年1月1日以来的秒数。
5. 地理位置信息采用经纬度表示，单位为度。
6. 测点信息中的坐标数据和楼层标高单位为米(m)。
7. 测点信息，楼层信息和时程数据(每个通道)被起始符和结束符包围。
8. 起始符(Begin)和结束符(End)设计为无法转成有效浮点数的32位整数，且不会被用作一般标识符，如：起始符为0x7FFFFFFF，结束符为0x8FFFFFFF。
9. 时程数据采用float32格式存储，确保精度和存储效率的平衡。