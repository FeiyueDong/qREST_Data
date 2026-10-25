# 发布说明

## v1.1.1 - 2026-09-21

- 将正式公共头文件统一到 `include/qrest_data/`，外部引用统一为
  `#include <qrest_data/...>`。
- 为独立项目和 products target 设置一致的 Module Version，并在独立构建时
  校验一致性；生成 C/C++ 通用的 `qrest_data/version.h`，让 GUI 和 C ABI
  运行时版本查询保持一致。
- 建立 `qrest_data_headers` Header-only target，集中传递公共 include 路径和
  `nlohmann_json` 依赖；移除全局 `include/qrest_data` 与 `src/` include root。
- 增加可由上层工程直接引入的 `xmake/products.lua`：仅定义正式产品，不引入
  本项目测试；GUI 在嵌入场景下默认关闭，独立项目仍默认构建 GUI 和测试。
- 完善 Windows `dllexport` / `dllimport` 宏，增加 `qrest_data_version()`。
- 修复 C API 反序列化时 File Header Magic 的 8 字节越界读取风险。
- 移除仓库级默认 MinGW 平台，并补齐 C API Packet Header 的原始字段映射，
  包括 `packet_type`、`body_size` 和 `checksum`。
- 更新 `release.py`，从当前 Xmake 输出生成带版本号的 `bin/`、`lib/`、
  `include/qrest_data/`、`doc/` 和 `examples/` 发布布局，并支持 Xmake install；
  Windows GUI 发布包必须通过 `windeployqt` 完成 Qt Quick/QML Runtime 部署。
- 增加纯 C API 编译/运行测试，以及仅消费安装前缀的独立 SDK smoke test。
- 保留 qREST Metadata Format Version `1.0.0` 和 DataPacket Protocol Version
  `1`，本次没有改变文件格式或外部格式导入算法。
- 延续 Data Tools GUI 的 View/Draft、Save As、外部格式导入、通道追加与验证
  工作流；移除 GUI Application Version 的硬编码。

## v1.0.1 - 2026-04-16

- 修正文件头 Magic 字段类型，增加 qREST CLI/GUI、C API 和初始发布脚本。
