# qREST_Data 重构收尾与版本管理完善开发方案

## 1. 目的与范围

本轮工作基于当前 `develop` 分支已经完成的目录重构继续收口，不再进行大规模架构调整。

当前方向总体正确：

- qREST 核心数据结构已经从原 `src/qrest_data/` 中抽离；
- `src/` 主要用于各功能模块和工具实现；
- C 接口已经进入公共 `include/` 区域；
- Xmake 已成为主要构建入口；
- 项目版本已经开始由根 `xmake.lua` 统一定义并生成版本头文件。

本轮目标是让以下内容完全一致：

1. Public API 目录结构；
2. Xmake include / dependency 关系；
3. Module Version 管理；
4. C API 导出与版本查询；
5. Release 打包；
6. 文档路径和版本信息；
7. 基础测试。

本轮不修改 qREST 文件存储格式本身，也不修改当前 Metadata Format Version `1.0.0`。

---

## 2. 当前需要收口的问题

### 2.1 Public Header 层级不完全统一

当前结构：

```text
include/
├─ data_packet.hpp
├─ file_header.hpp
├─ metadata.hpp
└─ qrest_data/
   └─ qrest_data.h
```

建议统一为：

```text
include/
└─ qrest_data/
   ├─ data_packet.hpp
   ├─ file_header.hpp
   ├─ metadata.hpp
   ├─ qrest_data.h
   └─ version.h
```

统一后所有外部引用使用：

```cpp
#include <qrest_data/data_packet.hpp>
#include <qrest_data/file_header.hpp>
#include <qrest_data/metadata.hpp>
#include <qrest_data/qrest_data.h>
```

不再使用：

```cpp
#include "metadata.hpp"
#include "data_packet.hpp"
#include "qrest_data.h"
```

内部模块自己的 Header 仍可继续使用：

```cpp
#include "validation.hpp"
#include "hdf5_reader.hpp"
```

---

### 2.2 Xmake include path 仍然过宽

当前 `src/xmake.lua` 同时加入：

```lua
add_includedirs("$(projectdir)/include")
add_includedirs("$(projectdir)/include/qrest_data")
```

统一 Public Header 后只保留：

```lua
add_includedirs("$(projectdir)/include")
```

另外，`src/qrest_data_tools/xmake.lua` 中：

```lua
add_includedirs(".")
add_includedirs("..")
```

应尽量缩小范围。不要让整个 `src/` 自动成为 include root。

各 target 应显式声明所需内部 include path。

---

### 2.3 建议建立 `qrest_data_core` Header-only target

项目核心本质上是：

```text
data_packet.hpp
file_header.hpp
metadata.hpp
```

建议在 Xmake 中显式表达：

```lua
target("qrest_data_core")
    set_kind("headeronly")

    add_headerfiles(
        "$(projectdir)/include/qrest_data/*.hpp"
    )

    add_includedirs(
        "$(projectdir)/include",
        {public = true}
    )

    add_packages(
        "nlohmann_json",
        {public = true}
    )
```

随后：

```text
qrest_data_lib
qrest_data_hdf5
qrest_data_tools_core
```

分别：

```lua
add_deps("qrest_data_core")
```

这样可以统一传递：

- Public include path；
- `nlohmann_json` public dependency；
- 核心 Header 的模块语义。

该项建议实现，但如希望保持最小改动，也可放到第二阶段。

---

## 3. 版本管理

### 3.1 明确三个版本概念

必须区分：

#### qrest_data Module Version

由根 `xmake.lua`：

```lua
set_version("1.1.1")
```

表示 qrest_data 软件模块版本。

#### qREST File / Metadata Format Version

当前：

```cpp
qrest_data::format::metadata_version{1, 0, 0}
```

表示 qREST Metadata / 文件格式版本。

本轮不修改。

#### DataPacket Protocol Version

DataPacket 中：

```text
version = 1
packet_type = 1
```

属于 Packet 协议版本。

本轮不修改。

因此以下状态完全合法：

```text
qrest_data Module Version : 1.4.0
qREST Format Version      : 1.0.0
Packet Protocol Version   : 1
```

---

### 3.2 修复版本模板

当前 `config/version.hpp.in` 中仍有：

```cpp
namespace qrest::dynamics
```

这是其他项目残留，必须修复。

考虑到项目存在正式 C API，建议改为 C/C++ 均可使用的：

```text
config/version.h.in
```

模板：

```c
#pragma once

#define QREST_DATA_VERSION_MAJOR ${VERSION_MAJOR}
#define QREST_DATA_VERSION_MINOR ${VERSION_MINOR}
#define QREST_DATA_VERSION_PATCH ${VERSION_ALTER}

#define QREST_DATA_VERSION_STRING "${VERSION}"
```

Xmake 生成：

```text
$(builddir)/generated/qrest_data/version.h
```

并确保相关 target 显式拥有 generated include path。

---

### 3.3 GUI 使用统一版本

当前 GUI 中硬编码：

```cpp
QCoreApplication::setApplicationVersion("1.0.1");
```

应改为：

```cpp
#include <qrest_data/version.h>

QCoreApplication::setApplicationVersion(
    QStringLiteral(QREST_DATA_VERSION_STRING)
);
```

之后只需要修改：

```lua
set_version("...")
```

GUI 版本即可自动同步。

---

## 4. C API 完善

### 4.1 修改 DLL Export Macro

当前：

```cpp
#define EXPORT __declspec(dllexport)
```

不适合作为正式 Public Header。

建议改为：

```c
#if defined(_WIN32)
#  if defined(QREST_DATA_BUILD_SHARED)
#    define QREST_DATA_API __declspec(dllexport)
#  else
#    define QREST_DATA_API __declspec(dllimport)
#  endif
#else
#  define QREST_DATA_API __attribute__((visibility("default")))
#endif
```

并将所有：

```c
EXPORT
```

替换为：

```c
QREST_DATA_API
```

`qrest_data_lib` target 构建时增加：

```lua
add_defines("QREST_DATA_BUILD_SHARED")
```

---

### 4.2 Public Header 引入版本信息

在：

```text
include/qrest_data/qrest_data.h
```

中加入：

```c
#include <qrest_data/version.h>
```

这样 C / C++ 调用方都可访问：

```c
QREST_DATA_VERSION_STRING
```

---

### 4.3 可选：增加 Runtime Version API

建议增加：

```c
QREST_DATA_API const char *qrest_data_version(void);
```

实现：

```cpp
const char *qrest_data_version(void) {
    return QREST_DATA_VERSION_STRING;
}
```

用于 Python、Rust、C# 等动态加载场景确认实际加载的 DLL / SO 版本。

该功能建议实现，但不作为阻塞项。

---

## 5. 修复 File Header Magic 拷贝问题

当前 `qrest_from_bytes()` 中：

```cpp
std::memcpy(
    out_data->file_header.magic,
    header.get_magic().c_str(),
    8
);
```

而 `FileHeader::get_magic()` 返回长度为 5 的 `"qREST"`。

从 `c_str()` 读取 8 字节存在越界读取风险。

建议改为：

```cpp
std::memset(out_data->file_header.magic, 0, 8);
std::memcpy(out_data->file_header.magic, "qREST", 5);
```

或者给 `FileHeader` 提供完整 8-byte accessor。

本轮至少应消除当前越界读取风险。

---

## 6. Release 系统更新

### 6.1 清理旧路径

`release.py` 不再使用：

```text
src/qrest_data/qrest_data.h
src/test_qrest_data/test_qrest_data.cpp
x64/Release
out/linux
```

全部调整为当前目录结构和 Xmake 输出目录。

---

### 6.2 Public Header 整目录复制

不要继续逐个 Header 手动复制。

建议：

```text
include/qrest_data/
        ↓
release/<platform>/include/qrest_data/
```

整体复制。

生成的：

```text
version.h
```

位于 build/generated 路径，因此打包时额外复制到：

```text
release/<platform>/include/qrest_data/version.h
```

---

### 6.3 发布包布局

建议：

```text
qrest_data-1.1.1-windows-x64/
├─ bin/
├─ lib/
├─ include/
│  └─ qrest_data/
├─ doc/
├─ examples/
├─ LICENSE.txt
├─ readme.md
└─ release.md
```

Linux 同理。

---

## 7. Install 支持

建议让项目支持：

```bash
xmake install -o <prefix>
```

生成：

```text
<prefix>/
├─ include/
│  └─ qrest_data/
├─ lib/
└─ bin/
```

至少安装：

```text
qrest_data.h
data_packet.hpp
file_header.hpp
metadata.hpp
version.h
qrest_data library
```

这样其他项目以后可以通过安装目录或 package 依赖，不再人工复制源码。

---

## 8. 文档更新

代码修改稳定后集中更新文档。

### 8.1 `readme.md`

需要：

- 更新旧的 `v1.0.1`；
- 更新“最后更新”；
- 增加当前目录结构；
- 明确 `Module Version != qREST File Format Version`。

推荐增加：

```text
include/qrest_data/
    Public qREST API

src/qrest_data_lib/
    C ABI implementation

src/qrest_data_hdf5/
    HDF5 support

src/qrest_data_tools/
    CLI / GUI / import / validation tools
```

---

### 8.2 `release.md`

更新当前版本，并记录：

- 目录结构重构；
- Public API `include/` 建立；
- Xmake 构建调整；
- 旧 VS 工程组织移除；
- Module Version 管理；
- Release 布局调整；
- 当前 GUI / Tools 功能更新。

长期建议升级为：

```text
CHANGELOG.md
```

按版本累计维护。

---

### 8.3 `doc/qrest_data_lib/interface.md`

更新：

- Public Header 路径；
- Include 示例；
- Module Version；
- Export Macro；
- Runtime Version API（如实现）。

引用统一为：

```c
#include <qrest_data/qrest_data.h>
```

避免继续使用旧源码路径。

---

### 8.4 GUI README / AGENTS

搜索并清理旧路径：

```text
src/qrest_data/metadata.hpp
src/qrest_data/qrest_data_tools/...
```

更新为：

```text
include/qrest_data/metadata.hpp
src/qrest_data_tools/...
```

同时说明：

> GUI Application Version 来自 qrest_data Module Version，不再硬编码。

---

### 8.5 Format Specification

不要因为 Module Version 更新到 `1.1.1` 就修改 qREST Format Version。

如果格式文档顶部当前存在容易混淆的：

```text
文档版本 v1.0.1
```

建议以后明确拆分：

```text
Document Revision: ...
qREST Format Version: 1.0.0
```

---

## 9. 测试补充

### 9.1 Public Header Compile Test

新增极小测试：

```cpp
#include <qrest_data/data_packet.hpp>
#include <qrest_data/file_header.hpp>
#include <qrest_data/metadata.hpp>
#include <qrest_data/qrest_data.h>
#include <qrest_data/version.h>
```

要求该测试不依赖任何 `src/` include path 即可编译。

---

### 9.2 Version Consistency Test

验证：

```text
QREST_DATA_VERSION_STRING
```

与 Xmake project version 一致。

如实现 Runtime Version API，再验证：

```c
qrest_data_version()
```

返回同一版本。

---

### 9.3 C API Magic Test

验证 `qrest_from_bytes()` 返回的：

```text
file_header.magic
```

完整 8 字节为：

```text
q R E S T 0 0 0
```

---

### 9.4 Existing Tests

确保：

```text
test_qrest_data_lib
test_qrest_data_hdf5
test_qrest_data_import_formats
```

继续通过。

至少执行：

```text
Windows / MSVC
Linux / GCC
```

如 MinGW 仍属于正式支持平台，再补充 MinGW 构建验证。

---

## 10. 推荐实施顺序

### Phase 1：Public API 收口

1. 将三个核心 Header 移入 `include/qrest_data/`；
2. 全局修改 include；
3. 删除 `include/qrest_data` 作为额外 include root；
4. 收紧 `src` 内部 include path；
5. 完整编译所有 target。

### Phase 2：Version System

1. 修复 `qrest::dynamics` 残留；
2. 建议切换到 `version.h.in`；
3. 生成 `qrest_data/version.h`；
4. GUI 使用统一版本；
5. C API 引入版本宏；
6. 可选增加 runtime version API。

### Phase 3：C API 修复

1. 修正 export/import macro；
2. 修复 File Header Magic 拷贝；
3. 更新 C API 测试。

### Phase 4：Release / Install

1. 修复 `release.py`；
2. Public Header 整目录复制；
3. 打包 generated version header；
4. 使用当前 Xmake build output；
5. 增加 `xmake install`。

### Phase 5：Documentation

统一更新：

```text
readme.md
release.md / CHANGELOG.md
doc/qrest_data_lib/interface.md
src/qrest_data_tools/gui/README.md
src/qrest_data_tools/gui/AGENTS.md
```

最后全仓库搜索并清理：

```text
src/qrest_data/
v1.0.1
qrest::dynamics
```

等旧内容。

---

## 11. 优先级

| 优先级 | 内容 |
|---|---|
| P0 | 修复 version template 中的 `qrest::dynamics` 残留 |
| P0 | GUI 去除硬编码版本 |
| P0 | 修复 `release.py` 旧路径 |
| P0 | 完成 Public Header 路径统一 |
| P1 | 修复 C API export/import macro |
| P1 | 修复 File Header Magic 8-byte 拷贝 |
| P1 | 收紧 Xmake include path |
| P1 | 更新 README / interface / GUI docs |
| P2 | `qrest_data_core` header-only target |
| P2 | Runtime Version API |
| P2 | `xmake install` |
| P2 | CHANGELOG / package 支持 |

---

## 12. 验收标准

本轮完成后应满足：

- `include/qrest_data/` 是唯一正式 Public Header 根目录；
- 外部代码统一使用 `#include <qrest_data/...>`；
- Public Header 编译不依赖任何 `src/` include path；
- qrest_data Module Version 只维护一次；
- GUI Application Version 自动来自 Module Version；
- C / C++ 均可读取统一版本宏；
- qREST File Format Version 独立保持 `1.0.0`；
- Windows C API 正确区分 `dllexport / dllimport`；
- `qrest_from_bytes()` 不再存在 Magic 越界读取；
- `release.py` 能针对当前 Xmake 构建结果生成发布包；
- 发布包中的 `include/qrest_data/` 可直接供外部项目使用；
- README、接口文档和 GUI 开发文档不再引用旧目录；
- Windows/MSVC 与 Linux/GCC 构建及测试通过。

---

## 13. 本轮不处理的内容

为避免重构范围继续扩大，本轮不处理：

- qREST Metadata Schema 大改；
- DataPacket Protocol Version 升级；
- qREST File Format Version 升级；
- GUI 架构重构；
- HDF5 / TDMS / MiniSEED 算法行为改变；
- C API 大规模重新设计；
- CMake 支持。

本轮重点仅是：

> 把已经完成的目录重构真正收口到 Public API、Build、Version、Release 和 Documentation 五个层面。
