# MiniSEED 时间精度修复（2026-10-27，尚未正式发版）

- 记录并使用 BTime / Blockette 1001 的编码精度，修复 75/120 Hz 非整数记录时长的误拒绝；记录连续性及跨通道对齐统一使用动态误差预算和共同采样网格检查。
- 新增 Blockette 100/1001、量化边界、微秒异常、缺口/重叠、乱序、跨通道和高采样率精度不足测试；保持现有格式与 C ABI。
- 模块版本维持 `1.1.1`，`.qrest` / metadata / DataPacket 协议版本不变。Windows/MSVC 发布验收仍待执行。
- 本轮实际证据及性能基线见 [验证记录](doc/reader_refactor_validation.md)。

# 外部读取更新（开发中）

新增标准 MiniSEED 2、SIT-4S DAT 和 SIT Gal TXT，共用 Steim-2 解码、物理量转换和通道映射；新增 `qrest_data_readers` 共享库及 `external_reader.h` C API。
MiniSEED 既有 SI 默认尺度不变，SIT 默认为 Gal。缺少校准时保留原始计数，qREST 导入严格校验单位、量纲、起点、方向及 Scale=1。
目录中的重复源通道现明确拒绝；旧测试中的双仪器目录使用不同站点标识。详见 [读取说明](doc/qrest_data_tools.md)。

# 发布说明

## 未发布

- 将已废弃功能及其历史资料集中归档至 `src/deprecated/`，不参与构建、安装或发布。
- 独立项目和 products 入口统一提供数据管理库、HDF5 桥接、导入格式、Tools Core
  和 CLI；保留共享数据编辑核心及其测试。
- 发布脚本只要求数据动态库和 CLI 产物。

## v1.1.1 - 2026-09-21

- 将正式公共头文件统一到 `include/qrest_data/`，外部引用统一为
  `#include <qrest_data/...>`。
- 为独立项目和 products target 设置一致的 Module Version，并在独立构建时
  校验一致性；生成 C/C++ 通用的 `qrest_data/version.h`，让编译期和 C ABI
  运行时版本查询保持一致。
- 建立 `qrest_data_headers` Header-only target，集中传递公共 include 路径和
  `nlohmann_json` 依赖；移除全局 `include/qrest_data` 与 `src/` include root。
- 增加可由上层工程直接引入的 `xmake/products.lua`：仅定义正式产品，不引入
  本项目测试；独立项目构建产品和本地测试。
- 完善 Windows `dllexport` / `dllimport` 宏，增加 `qrest_data_version()`。
- 修复 C API 反序列化时 File Header Magic 的 8 字节越界读取风险。
- 移除仓库级默认 MinGW 平台，并补齐 C API Packet Header 的原始字段映射，
  包括 `packet_type`、`body_size` 和 `checksum`。
- 更新 `release.py`，从当前 Xmake 输出生成带版本号的 `bin/`、`lib/`、
  `include/qrest_data/`、`doc/` 和 `examples/` 发布布局，并支持 Xmake install。
- 增加纯 C API 编译/运行测试，以及仅消费安装前缀的独立 SDK smoke test。
- 保留 qREST Metadata Format Version `1.0.0` 和 DataPacket Protocol Version
  `1`，本次没有改变文件格式或外部格式导入算法。

## v1.0.1 - 2026-04-16

- 修正文件头 Magic 字段类型，增加 qREST CLI、C API 和初始发布脚本。
