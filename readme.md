# qREST_Data项目

**qrest_data Module Version**: 独立项目版本由根 `xmake.lua` 定义；嵌入式
products 入口为生成 `<qrest_data/version.h>` 设置相同的 target-local 版本。

**最后更新**: 2026-09-22

本项目是qREST(Quick Response Evaluation for Safety Tagging)的一个子项目，主要负责数据管理。主要定义了两个协议：数据存储协议和数据传输协议。并提供一些简单的工具用于做处理和转换。

Module Version、qREST File / Metadata Format Version 和 DataPacket Protocol
Version 是三个独立概念。独立项目与 products 入口的 Module Version 必须一致，
独立构建会检查这一点；Metadata Format Version 仍为 `1.0.0`，DataPacket
Protocol Version 仍为 `1`。发布模块版本不会自动改变文件或数据包格式。

## 项目结构

```text
include/qrest_data/        Public qREST API
src/qrest_data_lib/        C ABI implementation
src/qrest_data_hdf5/       HDF5 support
src/qrest_data_tools/      CLI / GUI / import / validation tools
xmake/products.lua        Embedded products-only build entry
tests/products_smoke/     Independent products-entry integration test
```

外部代码统一从公共目录引用头文件，例如：

```cpp
#include <qrest_data/qrest_data.h>
#include <qrest_data/metadata.hpp>
```

## 构建与安装

正式发布平台为 Windows/MSVC 和 Linux/GCC。MinGW 可用于开发构建，但本版本
不提供正式 MinGW 发布包；macOS 也暂不属于正式发布范围。仓库不设置默认平台，
调用者必须通过 `xmake config -p ...` 显式选择。

Linux Debug 构建示例：

```bash
xmake config -p linux -m debug
xmake build
xmake run test_qrest_data_lib
xmake run test_qrest_data_c_api
xmake run test_qrest_data_hdf5
xmake run test_qrest_data_import_formats
```

安装到自定义前缀：

```bash
xmake install -o /path/to/prefix
```

生成发布包前先构建 Release，然后运行：

```bash
xmake config -p linux -m release
xmake build
python release.py linux --mode release
```

Windows 发布包生成时，`release.py` 会调用 `windeployqt` 部署 Qt Quick/QML
运行时；若工具不可用则直接失败。动态链接的 HDF5 DLL 必须由 Xmake 放入构建
输出目录，发布脚本会复制其中的全部 DLL。MSVC 运行时应通过官方 Visual C++
Redistributable 安装；发布脚本不复制开发机上的单个运行时 DLL。正式发布仍需在
未安装 Qt、Xmake、Visual Studio 或 HDF5 的干净 Windows 环境中进行启动验收。

Linux CLI/GUI 使用 `$ORIGIN/../lib` 查找发布包内的 qREST 动态库；Qt 与 HDF5
仍作为系统运行时依赖。

安装后的 SDK 可使用 `tests/installed_sdk_smoke/` 进行独立消费验证，该测试只
读取安装前缀中的 `include/` 和 `lib/`。

作为 Git 子模块集成时，上层工程只需引入产品入口：

```lua
includes("external/qrest_data/xmake/products.lua")

target("consumer")
    add_deps("qrest_data_headers")
```

`products.lua` 定义 `qrest_data_headers`、`qrest_data_lib`、HDF5 桥接库、
导入格式库、Tools Core 和 CLI，不引入本项目的测试 target。独立入口
`xmake.lua` 仍构建产品和测试，且默认包含 GUI；上层嵌入时 GUI 默认关闭，
可在配置时传入 `--qrest_data_enable_gui=y`。直接集成可运行
`tests/products_smoke/` 验证 target 边界和版本头。

## 1. 数据协议

### 1.1 数据存储协议

数据存储协议定义了qREST数据文件的结构和格式规范，见[数据存储协议](doc/qREST_DataStorage.md)。

### 1.2 数据传输协议

数据传输协议定义了qREST数据在不同系统之间传输的格式和规范，见[数据传输协议](doc/qREST_DataTransfer.md)。

## 2. 工具

### 2.1 qREST 命令行工具 (`qrest_data_tools_cli`)

`qrest_data_tools_cli` 提供命令行形式的数据读写能力。当前已迁移原有
`data_generator` 和 `data_loader` 的基础功能，并纳入 TDMS、modified
MiniSEED 与 HDF5 相关读写入口。

#### qREST 文件打包

由元数据 JSON 和时间主序文本矩阵生成符合数据存储协议的 `.qrest` 文件：

```bash
qrest_data_tools_cli pack <metadata.json> <data.txt> <output.qrest>
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
qrest_data_tools_cli extract <input.qrest> <metadata.json> <data.txt>
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
qrest_data_tools_cli inspect <input.qrest>
```

如需同时输出通道摘要：

```bash
qrest_data_tools_cli inspect <input.qrest> --channels
```

#### 数据一致性校验

校验完整 `.qrest` 文件：

```bash
qrest_data_tools_cli validate qrest <input.qrest>
```

校验元数据 JSON 和时间主序文本矩阵是否匹配：

```bash
qrest_data_tools_cli validate text <metadata.json> <data.txt>
```

#### 外部格式支持

TDMS 和 modified MiniSEED 文件本身只包含波形数据和有限采样信息，构建
`.qrest` 时仍需提供完整且匹配的 qREST `metadata.json`：

```bash
qrest_data_tools_cli import tdms <input.tdms> <metadata.json> <output.qrest>
qrest_data_tools_cli import mseed <input.mseed> <metadata.json> <output.qrest>
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
qrest_data_tools_cli export hdf5 <input.qrest> <output.h5>
qrest_data_tools_cli import hdf5 <input.h5> <output.qrest>
```

也可以单独校验外部格式文件是否可被当前解析器读取：

```bash
qrest_data_tools_cli validate tdms <input.tdms>
qrest_data_tools_cli validate mseed <input.mseed>
qrest_data_tools_cli validate hdf5 <input.h5>
```

### 2.2 qREST 可视化工具 (`qrest_data_tools_gui`)

`qrest_data_tools_gui` 提供可视化界面的 qREST 文件解析、元数据查看和数据包体导入导出能力。

### 2.3 qrest数据接口 (qrest_data)

提供了一个C语言接口的动态库，用于使用其他语言解析或生成qrest数据格式的字节流。接口定义见[数据接口](doc/qrest_data_lib/interface.md)。
