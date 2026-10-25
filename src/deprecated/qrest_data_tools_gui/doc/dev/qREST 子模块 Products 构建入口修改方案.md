# qREST 子模块 Products 构建入口修改方案

## 1. 修改目标

当前 qREST 使用以下三个独立仓库作为 Git submodule：

```text
external/
├── msl/
├── qrest_data/
└── qrest_dynamics/
```

三个项目仍应保持为**可以独立开发、构建、测试和发布的完整项目**。

同时，为方便 `qrest_module` 或其他上层项目复用，需要为每个子项目增加一个轻量的 **products-only 构建入口**：

```text
Standalone xmake.lua
    = 正式产品 + 本项目 tests/examples

products.lua
    = 仅正式产品
```

上层项目只引入 `products.lua`，不直接引入子项目根 `xmake.lua`。

---

## 2. 通用约定

建议三个项目统一增加：

```text
xmake/
└── products.lua
```

根 `xmake.lua` 调整为：

```lua
-- 项目自身配置
set_project(...)
set_version(...)
...

-- 正式产品
includes("xmake/products.lua")

-- 仅独立开发时使用
includes("tests")
includes("examples")
```

具体目录名称可根据各项目现状调整。

### Products 入口要求

`products.lua` 只允许定义：

* header-only library；
* static/shared library；
* 正式 CLI / GUI 工具；
* 正式产品所需内部依赖。

不得包含：

* `test_*`；
* benchmark；
* example；
* 仅供子项目开发使用的辅助 target。

此外，products 配置应可以从外部工程直接 `includes()`，因此：

* 不应假设当前 `$(projectdir)` 就是子项目根目录；
* 路径应优先根据 `os.scriptdir()` 等方式定位自身目录；
* 不应覆盖父项目的 `set_project()`、平台、编译模式等全局配置；
* 子项目版本仍由子项目自身维护，不继承 qrest_module 的版本号。

---

# 3. MSL

## 当前定位

MSL 是 header-only 数学支持库。

其正式产品只有：

```text
msl
```

现有 tests 仅用于 MSL 自身开发，不需要参与 qrest_module 构建。

## 修改目标

增加：

```text
MSL/
├── xmake.lua
├── xmake/
│   └── products.lua
├── include/
└── tests/
```

`products.lua` 只定义：

```lua
target("msl")
    set_kind("headeronly")
    add_includedirs(...)
    add_packages("eigen", {public = true})
```

根 `xmake.lua`：

```lua
set_project("MSL")
set_version(...)
...

includes("xmake/products.lua")
includes("tests")
```

### 预期结果

独立构建 MSL：

```text
msl
+ MSL tests
```

作为 qrest_module 子模块：

```text
msl
```

不引入任何测试 target。

---

# 4. qrest_data

## 当前定位

qrest_data 包含两类能力：

### 核心头文件

qrest_module 当前主要使用 `include/` 中的数据结构头文件。

建议明确提供：

```text
qrest_data_headers
```

作为 header-only target。

### 正式产品

当前正式产品包括但不限于：

```text
qrest_data_lib
qrest_data_hdf5
qrest_data_import_formats
qrest_data_tools_core
qrest_data_tools_cli
```

这些 target 即使当前 qrest_module 本身没有全部链接，也希望在 qrest_module 整体构建时一并生成。

测试 target 不应进入上层项目。

## 修改目标

建议整理为：

```text
qREST_Data/
├── xmake.lua
├── xmake/
│   └── products.lua
├── include/
├── src/
└── tests/
```

`products.lua` 负责定义：

```text
qrest_data_headers
qrest_data_lib
qrest_data_hdf5
qrest_data_import_formats
qrest_data_tools_core
qrest_data_tools_cli
```

其中：

```lua
target("qrest_data_headers")
    set_kind("headeronly")
    add_includedirs(<qrest_data/include>, {public = true})
```

正式 library/tool target 保留在各自原有 xmake 文件中，由 `products.lua` 统一 include。

## 需要清理的内容

当前部分产品 xmake 文件同时定义了测试，例如：

```text
qrest_data_hdf5
test_qrest_data_hdf5
```

以及：

```text
qrest_data_import_formats
qrest_data_tools_core
qrest_data_tools_cli
test_qrest_data_import_formats
```

应将测试 target 移出正式产品入口。

目标是：

```text
products.lua
    ↓
只能够到正式 target

root xmake.lua
    ↓
products + tests
```

### GUI

如果 data tools GUI 属于正式产品，可以继续保留，但建议作为可选产品，例如：

```text
qrest_data_enable_gui
```

避免 qrest_module 默认构建时被迫引入 Qt 等较重依赖。

---

# 5. qrest_dynamics

## 当前定位

qrest_dynamics 的核心产品是：

```text
qrest_dynamics
```

即结构动力学静态库。

当前根 `xmake.lua` 同时包含：

```text
qrest_dynamics
tests
examples
```

qrest_module 只需要静态库本身，不需要子项目 tests/examples。

## 修改目标

建议整理为：

```text
qrest_dynamics/
├── xmake.lua
├── xmake/
│   └── products.lua
├── include/
├── src/
│   ├── dynamics/
│   ├── test/
│   └── example/
└── ...
```

`products.lua` 只定义：

```lua
target("qrest_dynamics")
    set_kind("static")
    ...
```

根 `xmake.lua`：

```lua
set_project("qrest_dynamics")
set_version(...)
...

includes("xmake/products.lua")
includes("src/test")
includes("src/example")
```

### 版本注意事项

`qrest_dynamics` 自己生成的：

```text
dynamics/version.hpp
```

必须继续使用 qrest_dynamics 自己的版本。

不能因为被 qrest_module include，而错误继承：

```text
qrest_module VERSION
```

因此 products 配置中涉及版本生成时，应避免依赖父工程全局版本变量。

---

# 6. qrest_module 后续使用方式

三个子项目完成上述修改以后，qrest_module 后续只需要类似：

```lua
includes("external/msl/xmake/products.lua")
includes("external/qrest_data/xmake/products.lua")
includes("external/qrest_dynamics/xmake/products.lua")

includes("src")
```

形成统一构建图：

```text
qrest_module
│
├── msl
├── qrest_data_headers
├── qrest_data_lib
├── qrest_data_hdf5
├── qrest_data_tools_*
├── qrest_dynamics
└── qREST 自身 targets
```

而不会引入：

```text
MSL tests
qrest_data tests
qrest_dynamics tests
qrest_dynamics examples
```

---

# 7. 本轮非目标

本轮只修改三个子模块自身的构建组织，不处理：

* qrest_module 根 xmake 的正式接入；
* qrest_module 版本系统；
* release.py；
* VS 工程生成；
* CI；
* qrest_module 文档同步；
* 子模块版本统一。

这些内容在三个子项目 products 接口稳定后再统一处理。

---

# 8. 完成标准

三个子项目分别满足：

### 独立使用

```bash
cd <submodule>
xmake build
```

仍能正常进行该项目自身的完整开发构建，包括原有测试或示例。

### 嵌入使用

上层项目：

```lua
includes("<submodule>/xmake/products.lua")
```

只获得正式产品 target，不获得 tests/examples。

### 构建边界

最终达到：

```text
子项目负责定义自己的产品
上层项目负责选择消费哪些产品
测试和示例仍属于子项目自身
```

不在 qrest_module 中复制维护三个子项目的源码列表、依赖和具体构建规则。
