# 发布说明

## v1.1.1 - 2026-09-21

- 将正式公共头文件统一到 `include/qrest_data/`，外部引用统一为
  `#include <qrest_data/...>`。
- 以 Xmake `set_version()` 作为唯一 Module Version 来源，生成 C/C++ 通用的
  `qrest_data/version.h`，并让 GUI 和 C ABI 运行时版本查询保持一致。
- 建立 `qrest_data_core` Header-only target，集中传递公共 include 路径和
  `nlohmann_json` 依赖；移除全局 `include/qrest_data` 与 `src/` include root。
- 完善 Windows `dllexport` / `dllimport` 宏，增加 `qrest_data_version()`。
- 修复 C API 反序列化时 File Header Magic 的 8 字节越界读取风险。
- 更新 `release.py`，从当前 Xmake 输出生成带版本号的 `bin/`、`lib/`、
  `include/qrest_data/`、`doc/` 和 `examples/` 发布布局，并支持 Xmake install。
- 保留 qREST Metadata Format Version `1.0.0` 和 DataPacket Protocol Version
  `1`，本次没有改变文件格式或外部格式导入算法。
- 延续 Data Tools GUI 的 View/Draft、Save As、外部格式导入、通道追加与验证
  工作流；移除 GUI Application Version 的硬编码。

## v1.0.1 - 2026-04-16

- 修正文件头 Magic 字段类型，增加 qREST CLI/GUI、C API 和初始发布脚本。
