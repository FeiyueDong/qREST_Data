# qREST_Data项目

**版本**: v1.0.1
**最后更新**: 2026-04-16

本项目是qREST(Quick Response Evaluation for Safety Tagging)的一个子项目，主要负责数据管理。主要定义了两个协议：数据存储协议和数据传输协议。并提供一些简单的工具用于做处理和转换。

## 1. 数据协议

### 1.1 数据存储协议

数据存储协议定义了qREST数据文件的结构和格式规范，见[数据存储协议](doc/qREST_DataStorage.md)。

### 1.2 数据传输协议

数据传输协议定义了qREST数据在不同系统之间传输的格式和规范，见[数据传输协议](doc/qREST_DataTransfer.md)。

## 2. 工具

### 2.1 qREST 命令行工具 (`qrest_data_tools`)

`qrest_data_tools` 提供命令行形式的数据读写能力。当前已迁移原有
`data_generator` 和 `data_loader` 的基础功能，并纳入 TDMS、modified
MiniSEED 与 HDF5 相关读写入口。

#### qREST 文件打包

由元数据 JSON 和时间主序文本矩阵生成符合数据存储协议的 `.qrest` 文件：

```bash
qrest_data_tools pack <metadata.json> <data.txt> <output.qrest>
```

其中：
- `<metadata.json>`：包含数据元信息的JSON文件。
- `<data.txt>`：包含实际数据内容的文本文件，每行一个采样时刻，每列一个通道。
- `<output.qrest>`：生成的qrest数据文件。

常用选项：

- `--source-id N`：设置数据包 SourceID，默认 `1`。
- `--encoding N`：设置包体编码，默认 `0`，即 Float32。

#### qREST 文件解包

读取并解析 `.qrest` 文件，导出元数据 JSON 和时间主序文本矩阵：

```bash
qrest_data_tools extract <input.qrest> <metadata.json> <data.txt>
```

其中：
- `<input.qrest>`：要读取的qrest数据文件。
- `<metadata.json>`：导出的数据元信息JSON文件。
- `<data.txt>`：导出的数据内容文本文件。

常用选项：

- `--precision N`：设置导出文本小数精度，默认 `8`。

#### qREST 文件查看

查看 `.qrest` 文件头、包头和核心元数据信息：

```bash
qrest_data_tools inspect <input.qrest>
```

如需同时输出通道摘要：

```bash
qrest_data_tools inspect <input.qrest> --channels
```

#### 数据一致性校验

校验完整 `.qrest` 文件：

```bash
qrest_data_tools validate qrest <input.qrest>
```

校验元数据 JSON 和时间主序文本矩阵是否匹配：

```bash
qrest_data_tools validate text <metadata.json> <data.txt>
```

#### 外部格式支持

TDMS 和 modified MiniSEED 文件本身只包含波形数据和有限采样信息，构建
`.qrest` 时仍需提供完整且匹配的 qREST `metadata.json`：

```bash
qrest_data_tools import tdms <input.tdms> <metadata.json> <output.qrest>
qrest_data_tools import mseed <input.mseed> <metadata.json> <output.qrest>
```

导入时会校验外部数据的通道数、采样点数和采样率是否与 metadata 中的
`InstrumentInfo.ChannelNum`、`DataInfo.NPTS`、`DataInfo.DT` 一致；metadata
不会由工具自动推断或改写。

TDMS 常用选项：

- `--unit cm/s2|m/s2`：选择写入 qREST 的物理量单位，默认 `cm/s2`。
- `--sensitivity-mode acquisition|first|last|explicit`：选择灵敏度值，默认
  `acquisition`，即波形采集开始前生效的值。
- `--sensitivity N`：当 `--sensitivity-mode explicit` 时指定灵敏度原始值。
- `--counts`：直接写入原始计数值，不做物理量转换。

MiniSEED 常用选项：

- `--group-index N`：选择一个同步通道组，默认 `0`。
- `--include-dimensionless`：允许导入无量纲状态通道。

HDF5 接口采用本项目 `qrest_data_hdf5` 的受限布局，文件中已包含 qREST
metadata，可在 `.qrest` 和 `.h5` 之间转换：

```bash
qrest_data_tools export hdf5 <input.qrest> <output.h5>
qrest_data_tools import hdf5 <input.h5> <output.qrest>
```

也可以单独校验外部格式文件是否可被当前解析器读取：

```bash
qrest_data_tools validate tdms <input.tdms>
qrest_data_tools validate mseed <input.mseed>
qrest_data_tools validate hdf5 <input.h5>
```

### 2.2 qREST 可视化工具 (`qrest_data_tools_ui`)

`qrest_data_tools_ui` 提供可视化界面的 qREST 文件解析、元数据查看和数据包体导入导出能力。

### 2.3 qrest数据接口 (qrest_data)

提供了一个C语言接口的动态库，用于使用其他语言解析或生成qrest数据格式的字节流。接口定义见[数据接口](doc/qrest_data_lib/interface.md)。
