# qREST_Data v1.1.1 最终收尾修复方案

## 1. 目的

本轮为 `qREST_Data` v1.1.1 发布前的最后一次工程化收尾。

当前目录重构、Public API、版本管理、C ABI、Release 脚本和主要文档已经基本完成。本轮不再调整整体架构，只修复剩余的小问题，确保：

- 构建配置没有开发机偏好残留；
- C API 返回信息完整；
- Release 行为与正式支持平台一致；
- GUI 发布包具备可运行性；
- 文档与当前实现一致；
- 安装后的 Public API 可以被独立工程正常使用。

本轮不修改：

- qREST Metadata Format Version `1.0.0`；
- DataPacket Protocol Version `1`；
- GUI 业务架构；
- TDMS / MiniSEED / HDF5 算法逻辑；
- Metadata Schema。

---

# 2. 当前状态

当前 `develop` 已完成：

```text
include/qrest_data/        Public API
src/qrest_data_lib/        C ABI
src/qrest_data_hdf5/       HDF5
src/qrest_data_tools/      CLI / GUI / import / validation
```

并已建立：

```text
qrest_data_core
    ↓
qrest_data_lib
qrest_data_hdf5
qrest_data_tools_core
```

Module Version 由：

```lua
set_version("1.1.1")
```

统一定义，并生成：

```text
build/generated/qrest_data/version.h
```

GUI、C API 和 Release Script 已开始使用该统一版本。

本轮仅完成最后收尾。

---

# 3. P0：清理 Xmake 默认平台

当前根 `xmake.lua` 中仍存在：

```lua
set_config("plat", "mingw")
```

该配置属于开发环境偏好，不应该作为仓库级默认值。

## 修改

删除：

```lua
set_config("plat", "mingw")
```

保留：

```lua
set_allowedplats("windows", "linux", "macosx", "mingw")
```

平台由用户显式配置：

```bash
xmake config -p mingw
xmake config -p windows
xmake config -p linux
```

## 验收

以下命令均不会被仓库配置强制覆盖：

```bash
xmake config -p linux
xmake config -p mingw
xmake config -p windows
```

---

# 4. P0：补齐 C API Packet Header checksum

当前：

```c
qrest_c_packet_header_t
```

包含：

```c
uint32_t checksum;
```

但 `qrest_from_bytes()` 中尚未把实际 Packet Header 中的 checksum 写入输出结构。

## 修改

在解析 `packet_raw_bytes` 后读取：

```cpp
PacketHeaderPOD packet_header{};
std::memcpy(
    &packet_header,
    packet_raw_bytes.data(),
    sizeof(PacketHeaderPOD)
);
```

Packet Header 输出建议统一使用：

```cpp
out_data->packet_header.magic[0] = packet_header.magic[0];
out_data->packet_header.magic[1] = packet_header.magic[1];

out_data->packet_header.source_id = packet_header.source_id;
out_data->packet_header.version = packet_header.version;
out_data->packet_header.packet_type = packet_header.packet_type;
out_data->packet_header.channel_count = packet_header.channel_count;
out_data->packet_header.data_encodings = packet_header.data_encodings;
out_data->packet_header.sampling_rate = packet_header.sampling_rate;
out_data->packet_header.data_point_count = packet_header.data_point_count;
out_data->packet_header.timestamp = packet_header.timestamp;
out_data->packet_header.body_size = packet_header.body_size;
out_data->packet_header.checksum = packet_header.checksum;
```

也可以继续通过 `DataPacket` getter 获取业务字段，但至少：

```cpp
checksum
packet_type
body_size
```

应直接来自原始 Header，而不是重新推导或硬编码。

## 验收

`qrest_from_bytes()` 返回的：

```text
packet_header.checksum
```

必须与文件内原始 Packet Header 一致。

---

# 5. P0：补充 `<sstream>` 显式 include

当前：

```cpp
src/qrest_data_lib/qrest_data.cpp
```

使用：

```cpp
std::istringstream
```

但没有显式：

```cpp
#include <sstream>
```

目前可能依赖其他 Header 的间接包含。

## 修改

增加：

```cpp
#include <sstream>
```

原则：

> 每个 `.cpp/.hpp` 应直接包含自己使用的标准库 Header，不依赖传递 include。

---

# 6. P1：统一 Release 支持平台

当前 Xmake 允许：

```text
windows
linux
macosx
mingw
```

但 `release.py` 当前只接受：

```text
windows
linux
```

需要明确正式发布范围。

## 推荐方案

v1.1.1 正式 Release 首先定义为：

```text
Windows / MSVC
Linux / GCC
```

MinGW 作为开发构建平台，macOS 暂不作为正式 Release 平台。

此时：

- `release.py` 保持 `windows/linux`；
- README 明确“正式发布平台”；
- `set_config("plat", "mingw")` 必须删除；
- MinGW 不承诺 Release package。

如果希望 MinGW 也是正式发布平台，则需要单独扩展：

```python
choices=("linux", "windows", "mingw")
```

并明确：

- DLL；
- import library；
- runtime；
- package architecture；

的复制规则。

本轮推荐保持前一种简单方案。

---

# 7. P1：Windows GUI Runtime 部署

当前 `release.py` 可以复制：

```text
qrest_data_lib.dll
qrest_data_tools_cli.exe
qrest_data_tools_gui.exe
*.lib
```

但 GUI 还依赖 Qt Runtime。

正式 Windows 发布前必须确认：

```text
qrest_data_tools_gui.exe
```

可以在没有 Qt 开发环境的机器上启动。

## 建议流程

Windows Release：

```text
xmake build
    ↓
创建 package/bin
    ↓
复制 qREST DLL / EXE
    ↓
运行 windeployqt
    ↓
补充 HDF5 runtime（如动态链接）
    ↓
最终 package
```

示意：

```bash
windeployqt release/.../bin/qrest_data_tools_gui.exe
```

需要确认 Qt Quick / QML 相关运行时一并被部署，例如：

```text
Qt6Core.dll
Qt6Gui.dll
Qt6Qml.dll
Qt6Quick.dll
platforms/qwindows.dll
qml/...
```

## HDF5

检查当前 Xmake 的 HDF5 链接方式。

如果是动态链接，还需要在 Release package 中包含对应：

```text
hdf5.dll
```

及其运行时依赖。

如果是静态链接，则无需额外处理。

## 验收

在一台未安装：

- Qt；
- Xmake；
- Visual Studio；
- HDF5；

的干净 Windows 环境中启动：

```text
qrest_data_tools_gui.exe
```

并验证：

- GUI 正常打开；
- Help 文档可打开；
- qREST 文件可读取；
- HDF5 Import/Export 可用。

---

# 8. P1：检查 Xmake dependency 可见性

当前：

```lua
qrest_data_tools_core
    add_deps("qrest_data_core")
```

其内部公开 Header 会直接包含：

```cpp
#include <qrest_data/metadata.hpp>
```

建议将依赖语义调整为：

```lua
add_deps("qrest_data_core", {public = true})
```

这样如果后续其他 target 直接消费 `qrest_data_tools_core`，可以自然得到：

```text
include/qrest_data
generated/version header
nlohmann_json
```

的依赖传播。

同理检查其他 target：

```text
qrest_data_hdf5
qrest_data_tools_core
qrest_data_lib
```

是否需要 Public 或 Private dependency。

本轮原则：

- Header 中暴露的依赖 → `public = true`
- 仅 `.cpp` 中使用的依赖 → private

不需要为了追求形式统一而全部设为 public。

---

# 9. P1：更新根 `AGENTS.md`

当前根 `AGENTS.md` 仍存在旧描述，例如：

```text
project/ directory contains generated Visual Studio project files
```

而当前仓库已不再采用该结构。

同时旧示例路径仍可能引用：

```text
resource/kunming2/
```

应改为：

```text
resource/qrest_data/kunming2/
```

## 建议内容

Project Structure 更新为：

```text
include/qrest_data/        Public C/C++ headers
src/qrest_data_lib/        C ABI shared library
src/qrest_data_hdf5/       HDF5 bridge
src/qrest_data_tools/      CLI / GUI / import / validation
resource/qrest_data/       qREST examples
resource/dev/data/         external-format test fixtures
doc/                       specifications and interface documentation
```

Build 说明以 Xmake 为唯一正式入口。

删除：

```text
project/
Visual Studio generated project
```

等已不存在内容。

如果当前仓库已不再维护：

```text
vcpkg.json
vcpkg-configuration.json
```

也删除对应说明。

---

# 10. P1：修正 GUI README 中旧 Channel 行为描述

当前 GUI README 的 Near-Term Improvement Points 中仍有类似：

```text
Channel add/delete/duplicate is currently locked after packet body data exists
```

与当前实现不完全一致。

## 建议修改

改为：

> After packet data exists, metadata-only Add and Duplicate remain locked.  
> Channel count changes must use Data → Add Channels or Delete Channel + Data so Metadata and Packet data remain synchronized.

中文含义：

```text
已有 Packet 数据后：

Metadata-only Add       → 禁止
Metadata-only Duplicate → 禁止

Add Channels + Data     → 支持
Delete Channel + Data   → 支持
```

这样与当前实际行为一致。

---

# 11. P2：文档建立单一源

目前文件存储规范存在多个副本：

```text
doc/qREST_DataStorage.md
doc/qREST文件存储格式规范.md
src/qrest_data_tools/gui/doc/qrest_data_storage.md
doc/qREST文件存储格式规范.html
```

本轮可以先不彻底删除，但建议开始收口。

## 推荐 canonical source

建议保留：

```text
doc/qREST_DataStorage.md
doc/qREST_DataTransfer.md
```

作为唯一 Markdown Source of Truth。

其他内容由它们生成或复制。

例如：

```text
doc/qREST_DataStorage.md
        ├─ GUI bundled file
        └─ generated HTML
```

## GUI

当前：

```text
src/qrest_data_tools/gui/doc/qrest_data_storage.md
```

属于人工副本。

长期建议在构建时复制：

```text
doc/qREST_DataStorage.md
```

到 GUI resource build directory，再加入 qrc。

第一步如果不想调整 QRC 构建流程，至少增加注释：

```text
该文件由 doc/qREST_DataStorage.md 同步生成，不应独立修改
```

---

# 12. P2：取消 README / Interface 中当前 Module Version 的手工复制

代码版本已经做到：

```text
xmake set_version()
        ↓
version.h
        ↓
GUI / C API / release.py
```

但文档中仍手写：

```text
v1.1.1
```

例如：

```text
readme.md
doc/qrest_data_lib/interface.md
```

这仍可能在下次发布时忘记同步。

## 推荐方案

README 不再写：

```text
Current Module Version: v1.1.1
```

改成：

> Current module version is defined by root `xmake.lua` and exposed through `<qrest_data/version.h>`.

Interface 文档同理。

历史版本只在：

```text
release.md / CHANGELOG.md
```

中保留。

这样当前版本真正只有一个 source of truth。

---

# 13. P2：增加真正的 Public SDK Smoke Test

当前：

```text
test_qrest_data_lib
```

已经可以验证 Public Header，但它仍在本仓库 dependency graph 中编译。

正式发布前建议增加独立安装测试。

## 流程

```bash
xmake install -o /tmp/qrest_data_sdk
```

然后建立一个临时独立工程：

```text
sdk_smoke/
├─ main.cpp
└─ xmake.lua
```

`main.cpp`：

```cpp
#include <qrest_data/data_packet.hpp>
#include <qrest_data/file_header.hpp>
#include <qrest_data/metadata.hpp>
#include <qrest_data/qrest_data.h>
#include <qrest_data/version.h>

int main() {
    return qrest_data_version() ? 0 : 1;
}
```

该工程只能使用：

```text
/tmp/qrest_data_sdk/include
/tmp/qrest_data_sdk/lib
```

不能引用当前仓库的：

```text
src/
build/generated/
```

## 验收

独立工程可以：

- compile；
- link；
- run。

---

# 14. P2：增加纯 C API 编译测试

因为：

```text
qrest_data.h
```

是正式 C API，应验证它能被纯 C 编译器包含。

新增：

```text
test_public_c_api.c
```

示例：

```c
#include <qrest_data/qrest_data.h>

int main(void) {
    const char *version = qrest_data_version();
    return version ? 0 : 1;
}
```

使用 C 编译，而不是 C++。

目的：

- 防止 Public C Header 引入 C++ 类型；
- 防止 `extern "C"` 或宏设计退化；
- 验证 `version.h` 也是 C-compatible。

---

# 15. P2：补充 checksum 测试

在：

```text
test_qrest_data_lib
```

中增加：

1. 序列化生成 Packet；
2. 从原始字节读取 `PacketHeaderPOD.checksum`；
3. 调用 `qrest_from_bytes()`；
4. 比较：

```cpp
parsed_data->packet_header.checksum
```

与原始值。

例如：

```cpp
if (parsed_data->packet_header.checksum != raw_header.checksum) {
    throw std::runtime_error("Packet checksum mismatch");
}
```

同时建议顺便验证：

```text
packet_type
body_size
version
```

避免 C API Header 映射遗漏。

---

# 16. P2：最低限度 CI

当前仓库没有自动 Commit Status。

本轮不要求完整 Release CI，但建议至少建立 Build/Test CI。

## Linux

```text
xmake config -p linux -m release
xmake build
test_qrest_data_lib
test_qrest_data_hdf5
test_qrest_data_import_formats
```

## Windows

```text
xmake config -p windows -m release
xmake build
test_qrest_data_lib
```

如外部数据测试依赖较复杂，可先只跑核心测试。

CI 第一阶段目标不是自动发布，而是防止：

- include path 断裂；
- generated version Header 失效；
- Public API 编译失败；
- 平台条件代码退化。

---

# 17. Release Script 最终检查

当前 `release.py` 已经能够：

```text
读取 generated version
复制 bin/lib
复制 public header
复制 generated version.h
复制 doc
复制 examples
```

本轮需要再检查：

### 必须

- Release package 目录命名正确；
- `qrest_data_lib` 缺失时失败；
- `version.h` 缺失时失败；
- Public Header 完整；
- Windows/MSVC `.lib/.dll` 复制正确；
- Linux `.so/.a` 复制正确。

### 可选增强

增加参数：

```text
--skip-gui
--skip-doc
```

不是本轮必须。

---

# 18. 最终文档状态

本轮结束后建议保持：

```text
readme.md
    项目简介 / 构建 / 使用 / 目录结构

release.md 或 CHANGELOG.md
    软件历史版本

doc/qREST_DataStorage.md
    qREST 文件格式

doc/qREST_DataTransfer.md
    DataPacket 协议

doc/qrest_data_lib/interface.md
    C API

src/qrest_data_tools/gui/doc/helper.md
    GUI User Guide
```

版本职责：

```text
Module Version
    → xmake.lua

Metadata Format Version
    → metadata.hpp / format specification

Packet Protocol Version
    → data_packet.hpp / transfer specification

Document Revision
    → documentation only
```

三者不能再次混用。

---

# 19. 实施顺序

建议严格按以下顺序完成。

## Step 1

修复代码：

- 删除 `set_config("plat", "mingw")`
- 补 `<sstream>`
- 补 Packet checksum 映射
- 如需要调整 `qrest_data_tools_core` dependency public 属性

## Step 2

修复测试：

- checksum test
- Public Header test
- pure C API compile test

## Step 3

修复文档：

- `AGENTS.md`
- GUI README
- README / interface 中当前版本手工值

## Step 4

验证 Release：

- Linux package
- Windows package
- Windows Qt runtime
- HDF5 runtime

## Step 5

独立 SDK smoke test。

## Step 6

如时间允许，增加最小 CI。

---

# 20. 优先级

| 优先级 | 内容 |
|---|---|
| P0 | 删除仓库级 `set_config("plat", "mingw")` |
| P0 | 补齐 C API checksum |
| P0 | `qrest_data.cpp` 补 `<sstream>` |
| P1 | 明确 Release 正式支持平台 |
| P1 | Windows Qt/HDF5 Runtime 部署 |
| P1 | 更新根 `AGENTS.md` |
| P1 | 修正 GUI README 旧 Channel 描述 |
| P1 | 检查 Xmake public/private dependency |
| P2 | 文档单一 Markdown Source |
| P2 | README / Interface 去除当前版本硬编码 |
| P2 | Installed SDK smoke test |
| P2 | Pure C API compile test |
| P2 | CI |

---

# 21. 最终验收标准

v1.1.1 收尾完成后应满足：

- 根 Xmake 不强制默认平台；
- Linux / Windows 可以显式配置并构建；
- Public Header 全部通过 `<qrest_data/...>` 使用；
- `qrest_data_version()` 与 `QREST_DATA_VERSION_STRING` 一致；
- C API 返回的 Packet Header 字段完整，包括 `checksum`；
- `qrest_data.cpp` 不依赖间接标准库 include；
- qREST Format Version 仍为 `1.0.0`；
- Packet Protocol Version 仍为 `1`；
- Release package 可以被独立使用；
- Windows GUI 在无开发环境机器上可以启动；
- Public SDK 安装后可以被独立 C++ 项目编译链接；
- `qrest_data.h` 可以被纯 C 编译器包含；
- README、AGENTS、GUI README 不再描述旧目录或旧 Channel 行为；
- 当前 Module Version 不再在多个代码位置人工维护。

---

# 22. 本轮结束后的建议

完成本轮后，不建议继续对 v1.1.1 做结构性改造。

后续新工作应进入正常版本演进，例如：

```text
1.1.2
    bug fix

1.2.0
    backward-compatible feature

2.0.0
    breaking public API / format change
```

v1.1.1 的目标应停留在：

> Public API、Build、Version、Release、Documentation 已经形成稳定闭环，可以作为后续 qREST 模块依赖和 Data Tools 开发的基线版本。
