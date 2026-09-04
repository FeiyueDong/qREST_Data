# qREST Data Tools 下一阶段优化计划

## 1. 阶段目标

目前 `qrest_data_tools_gui` 已经完成主要功能和基础架构，包括：

* qREST 文件新建、打开、Draft 编辑和 Save As；
* Building / Channels / Data / Validation 页面；
* 结构化 Metadata 编辑；
* Raw JSON / Binary Viewer / Packet Inspector；
* Channel 管理；
* 2.5D Sensor Layout；
* GUI 与 Core 共用的基础 Validation；
* Metadata 扩展字段保存；
* Overview Dashboard。

当前版本已经具备较完整的工具雏形。下一阶段不再以大规模架构修改为重点，而应转向：

> **增强实际数据导入能力、降低普通用户理解 qREST 字段的门槛、提高 Sensor Layout 的检查能力，并补充正式工具所需的帮助和安全性功能。**

现有 GUI 已明确依赖 `qrest_data_tools_core`，而 Core 已经具备 TDMS、modified MiniSEED、HDF5 等外部数据读写能力，因此下一阶段应优先复用现有 Core，而不是在 GUI 中重新实现格式解析。

---

# 2. 总体开发原则

本阶段继续遵守以下原则：

```text
qrest_data_lib
       ↓
qrest_data_tools_core
       ↓
QrestDocument / QrestViewModel
       ↓
QML
```

其中：

* 文件格式解析、Validation、外部格式读取等通用逻辑尽量放 Core；
* GUI 负责工作流、参数收集、预览和交互；
* 不在 QML 中复制数据格式和算法逻辑；
* 已有稳定功能尽量保持不变；
* 优先解决实际使用体验，而不是继续增加低价值复杂功能。

---

# 3. 外部数据格式导入接入 GUI

这是下一阶段最重要的新功能。

当前 Core 已支持：

* TDMS；
* modified MiniSEED；
* HDF5；
* 单文件和部分目录/collection 导入；
* ExternalDataset；
* 外部 Dataset 与 Metadata 兼容性检查。

GUI 应正式暴露这些能力。

## 3.1 Data 菜单调整

建议改为：

```text
Data
├─ Import Data Body
│  └─ Text / CSV...
│
├─ Import External Data
│  ├─ TDMS...
│  ├─ Modified MiniSEED...
│  └─ HDF5...
│
├─ Export
│  ├─ Text...
│  └─ HDF5...
│
├─ Import Metadata JSON...
└─ Export Metadata JSON...
```

普通 TXT/CSV 导入继续保留现有逻辑。

---

# 4. 外部数据导入工作流

不建议用户选完文件后直接修改当前 Draft。

推荐采用：

```text
Select External File / Directory
            ↓
Load / Inspect
            ↓
External Dataset Preview
            ↓
Channel Mapping
            ↓
Compatibility Check
            ↓
Confirm
            ↓
Apply to Draft
```

## 4.1 Preview

至少显示：

```text
Format
File / Directory
Channel Count
Sample Count
Sample Rate
Detected Channel Labels
```

例如：

```text
Format          TDMS
Channels        3
Samples         180000
Sampling Rate   100 Hz

Detected:
N
E
Z
```

先让用户知道程序解析到了什么，再真正导入。

---

# 5. 外部 Channel Mapping 必须重新设计

这是 GUI 接入外部数据前需要解决的重要问题。

当前 collection import 会根据文件序号和方向构造：

```text
X1 / Y1 / Z1
X2 / Y2 / Z2
...
```

再直接查找同名 Metadata `ChannelID`。同时目前 Core 对这一路径会拒绝 `UNKNOWN` ChannelID。

但当前 qREST GUI 已明确采用：

> ChannelID 是用户自由定义的通道/硬件标识，不承担通道顺序或方向编码职责。

因此不能继续把 `X1/Y1/Z1` 作为外部数据导入的硬性规则。

## 5.1 建议引入独立 Mapping

概念上建立：

```text
ExternalChannelMapping
```

例如：

```text
External Channel          qREST Channel
------------------------------------------------
file01 / N          →     Channel 1 / ABC001
file01 / E          →     Channel 2 / UNKNOWN
file01 / Z          →     Channel 3 / SENSOR03
```

Mapping 根据：

```text
qREST ChannelNo / index
```

确定真正的数据列目标，而不是依赖 ChannelID 名称。

---

# 6. 自动 Mapping

现有 `X1/Y1/Z1` 推断逻辑仍然有价值，但应降低为：

> Auto Mapping heuristic

而不是格式要求。

可以尝试根据：

* 外部文件顺序；
* N/E/Z、X/Y/Z 等方向；
* qREST Channel Azimuth；
* Channel 数量；
* 位置/高度；

产生一个推荐 Mapping。

之后在 GUI 中让用户确认。

### 第一版

不需要设计非常复杂的自动算法。

只需要：

1. 能产生合理默认 Mapping；
2. 用户可以手动修改目标 Channel；
3. Mapping 必须一一对应；
4. 导入前检查是否存在未映射或重复映射。

---

# 7. TDMS / MiniSEED 参数界面

不要把所有 Core 参数直接平铺在主界面。

建议：

```text
Import TDMS
-------------------------
Input

Target Unit
Sensitivity
Time Verification

[ Advanced ▼ ]

[Preview] [Cancel]
```

Advanced 中再放：

* sensitivity selection；
* explicit sensitivity；
* storage scale；
* post scale；
* raw counts；
* time check。

MiniSEED 类似：

```text
Group
Include Dimensionless
Time Continuity Check
```

Core 当前已经具备对应 Options，不要在 GUI 再定义一套不同语义。

---

# 8. 外部数据单位

目前 Metadata Distance Unit 可以是：

```text
m
cm
mm
```

时间单位固定：

```text
s
```

而 TDMS Core 当前主要支持：

```text
m/s²
cm/s²
```

下一阶段建议逐渐统一为：

> 外部数据导入后自动转换到当前 qREST Metadata 对应的目标物理单位。

普通用户最好只需要看到：

```text
Target Unit: cm/s²
```

而不需要同时理解：

* Metadata unit；
* import unit；
* post scale。

`post_scale` 等仍可以保留在 Advanced 中。

---

# 9. 外部导入异步执行

TDMS / MiniSEED / HDF5 文件可能很大。

GUI 不应直接在主线程运行：

```cpp
load_tdms_collection(...)
load_mseed_collection(...)
load_hdf5_dataset(...)
```

否则窗口会冻结。

建议采用：

```text
GUI Thread
    ↓
Worker / QtConcurrent
    ↓
ExternalDataset
    ↓
GUI Preview
```

界面显示：

```text
Loading TDMS...
```

如果实现成本允许，增加：

```text
Cancel
```

ExternalDataset 完成读取以后再进行 Mapping 和最终 Apply。

---

# 10. Help 菜单

目前应用已经进入适合增加正式 Help 系统的阶段。

新增：

```text
Help
├─ User Guide
├─ qREST File Format Specification
├─ Project Homepage
└─ About qREST Data Tools
```

---

# 11. User Guide

新增独立用户手册，例如：

```text
doc/qrest_data_tools_user_guide.md
```

不要把现有 GUI README 当用户手册使用。

当前 GUI README 主要描述项目结构、数据流和开发约束，更适合开发人员。

User Guide 应主要说明实际操作：

```text
Introduction
Create qREST File
Open Existing File
Edit Draft
Save As
Building Metadata
Channel Configuration
Sensor Layout
Data Import
External Data Import
Validation
Advanced JSON
Binary Viewer
Packet Inspector
```

---

# 12. File Format Documentation

Help 菜单应能够直接查看项目现有的 qREST 数据格式规范。

建议把：

```text
User Guide
File Format Specification
```

作为资源随 GUI 一同分发，使程序离线也能查看。

可以建立：

```text
DocumentViewerDialog.qml
```

使用：

```text
ScrollView
    └─ TextEdit / TextArea
```

支持 Markdown 显示即可。

不需要引入 Qt WebEngine。

---

# 13. Project Homepage

增加：

```text
Help → Project Homepage
```

使用：

```qml
Qt.openUrlExternally(...)
```

打开 qREST_Data GitHub 项目页面。

---

# 14. About Dialog

新增简单 About Dialog。

建议包含：

```text
qREST Data Tools
Application Version

Supported qREST Format Version

Project Homepage
License
Copyright
```

同时在 `main.cpp` 中正式设置：

```cpp
ApplicationName
ApplicationDisplayName
ApplicationVersion
OrganizationName
```

目前 main.cpp 已设置应用图标，可以在这个基础上补齐应用元信息。

---

# 15. Field Help 系统

下一阶段开始建立统一字段帮助机制。

目标：

> 用户无需频繁打开格式规范，也能理解 Building / Channels / Data 中各字段的含义。

例如鼠标停留：

```text
Azimuth  ⓘ
```

显示：

```text
Measurement direction of the channel.

0° = +Y
90° = +X
Vertical channels use -1 internally.
```

## 15.1 实现原则

不要在各 QML 页面写死大量 Tooltip 文本。

采用统一的：

```text
Field Help Mapping
```

例如后续提供：

```text
gui/help/field_help.json
```

映射：

```text
Metadata stable field path
        ↓
Display label / summary / details / range / example
```

例如：

```text
BuildingInfo.GeoLocation.NorthAngle
InstrumentInfo.Channels[].Azimuth
DataInfo.DT
```

---

# 16. Field Help 本轮工作范围

本轮只建立基础设施：

```text
FieldHelpRegistry
FieldLabel.qml
fieldKey
ToolTip
```

完整：

```text
field_help.json
```

内容暂不在本计划中定义。

后续会单独讨论并提供完整映射表文件。

因此现阶段不要自行大量编写或猜测 Field Help 文案。

---

# 17. Field Label 交互

建议统一显示为：

```text
Azimuth ⓘ
North Angle ⓘ
Scale ⓘ
```

鼠标停留约：

```text
500~700 ms
```

显示 Tooltip。

Tooltip：

* 控制在合理宽度；
* 支持换行；
* 优先显示 summary；
* 有 details/range/example 时再追加。

只读字段也需要帮助，例如：

```text
Bounding Box
ChannelNum
Direction
```

这样用户可以理解为什么它们不能修改。

---

# 18. Field Help 与 Validation 的未来联动

Field Help Registry 可以保留：

```text
page
fieldKey
```

信息。

未来 Validation Issue 也可以携带 `fieldKey`。

从而实现：

```text
Validation Error
      ↓ click
Go to Channels
      ↓
Focus Azimuth
```

但本轮可以只预留结构，不要求立即实现完整跳转。

---

# 19. Sensor Layout：继续使用当前 2.5D 方案

当前阶段不引入真正 Qt Quick 3D。

当前 Sensor Layout 已经实现：

* building wireframe；
* Sensor marker；
* direction arrow；
* XYZ 2.5D projection；
* selection linkage；
* coordinate / North indication。

下一阶段继续完善这一方案。

---

# 20. 楼层平面增加半透明填充

为提高不同楼层之间的视觉区分度，在现有 floor outline 内增加统一半透明填充。

原则：

```text
same color
+
same transparency
```

不要按楼层使用不同颜色。

例如可以采用淡蓝灰色、低透明度。

推荐视觉强度大致：

```text
alpha ≈ 0.12 ~ 0.20
```

具体颜色由现有 UI 风格决定。

---

# 21. Floor Fill 绘制顺序

建议：

```text
Floor Filled Polygons
        ↓
Floor Outlines
        ↓
Vertical Edges
        ↓
Sensors
        ↓
Direction Arrows
        ↓
Selected Sensor
```

避免半透明楼板覆盖传感器和重要边线。

如果多个楼板存在叠加，应按稳定的：

```text
far → near
```

顺序绘制，使透明叠加更自然。

---

# 22. Sensor Layout 增加正交视图

相比立刻实现真正 3D，更优先增加：

```text
[ Isometric ]
[ Plan ]
[ X-Z ]
[ Y-Z ]
```

---

## Isometric

保留当前视图。

用途：

> 快速理解整体建筑和 Sensor 空间布置。

---

## Plan

显示：

```text
X-Y
```

用于准确检查：

* Sensor X；
* Sensor Y；
* Azimuth；
* Footprint；
* North。

---

## X-Z

用于准确检查：

```text
X
Z
```

以及 Sensor 所在高度。

---

## Y-Z

用于准确检查：

```text
Y
Z
```

---

# 23. Floor Filter

Plan View 可以进一步增加：

```text
Elevation
[ All ▼ ]
```

例如：

```text
All
0.0 m
3.6 m
7.2 m
...
```

选择某层以后，只显示该高度附近的 Sensor。

这对建筑监测数据的检查尤其有价值。

第一版可以在正交视图完成后再实现。

---

# 24. Sensor Hover

建议增加 Sensor hover 信息。

例如：

```text
Channel 12
ID: ABC001
Device: Accelerometer

X: 12.3 m
Y: -5.6 m
Z: 21.6 m

Azimuth: 90°
Direction: X
```

现有 Selected Channel 和图形已经存在联动，因此 hover 可以直接使用当前 geometry/channel 数据。

---

# 25. 暂不实现真正 3D

暂时不引入：

```text
Qt Quick 3D
free camera
perspective camera
3D picking
```

原因：

* 当前主要需求是 Metadata 检查而不是模型展示；
* Plan / Elevation 在精确判断 Sensor 坐标时更有效；
* 真 3D 会增加 Camera / depth / picking 复杂度。

以后如果加入，建议作为：

```text
3D
```

第五种 View，而不是替代二维和 2.5D 视图。

---

# 26. Save As 文件安全修复

这是下一阶段应优先处理的一个隐藏问题。

当前 `QrestDocument::saveAs()` 会直接写入用户选择的目标路径，而没有明确禁止目标路径等于原始 Source File。

因此可能：

```text
Open A.qrest
→ Edit Draft
→ Save As
→ A.qrest
```

从而覆盖原文件。

这违反：

> Existing qREST source file must remain untouched.

的设计原则。

---

# 27. Save As 路径保护

EditDraft 模式下：

```text
targetPath == originalSourcePath
```

必须拒绝保存。

提示：

```text
The original source file cannot be overwritten.
Please choose a different output path.
```

路径比较应尽量规范化：

```text
absolute path
canonical path where possible
Windows case handling
. / ..
```

NewDraft 不受这一限制。

---

# 28. qREST I/O 重复逻辑

当前 Core 已提供：

```cpp
read_qrest_file()
write_qrest_file()
```

但 `QrestDocument` 仍单独处理一套：

```text
FileHeader
Metadata
DataPacket
serialization
```

当前暂不要求立即重构，因为 GUI 还需要 Draft 和 Packet 编辑能力。

但建议作为后续技术债：

> 逐步抽取 GUI 与 CLI 共用的 document parse/serialize helper。

避免未来格式修复只修改其中一条路径。

---

# 29. 自动化测试补充

随着项目趋于稳定，建议逐步增加测试覆盖。

当前 tests 主要集中在 import formats。

建议补充：

```text
test_metadata.cpp
test_validation.cpp
test_qrest_file.cpp
```

重点测试：

### Metadata

* JSON round trip；
* extension preservation；
* DeviceType；
* Header / Version。

### Validation

* duplicate ID；
* multiple UNKNOWN；
* Draft / Final；
* packet mismatch；
* sampling rate；
* timestamp。

### qREST file

* read/write round trip；
* supported encodings；
* CRC；
* malformed size。

### External Import

在 Mapping 重构完成后：

* arbitrary ChannelID；
* UNKNOWN ChannelID；
* manual mapping；
* duplicate mapping；
* missing mapping。

---

# 30. 大文件性能

当前：

```text
Data Table
QrestDocument
```

仍然以完整内存数据为主。

GUI README 也已经把 large-file behavior 列为后续改进项。

下一阶段暂不作为主要开发目标。

只需要：

* 外部数据导入避免阻塞 UI；
* 不再新增明显的一次性超大字符串/对象。

以后再考虑：

```text
lazy loading
chunked data
memory mapping
streaming
```

---

# 31. 小型桌面软件体验优化

如果主要功能完成后有余力，可以增加：

```text
Ctrl+N
Ctrl+O
Ctrl+Shift+S
```

等标准快捷键。

以及使用：

```text
QSettings
```

保存：

* 上次打开目录；
* Window size；
* Channels SplitView 比例；
* 最近文件路径。

Dirty Draft 关闭 Dialog 以后也可以考虑增加：

```text
Save As
Discard
Cancel
```

目前这些都属于体验优化，不是核心开发目标。

---

# 32. 推荐实施顺序

## Phase 1：安全和 Core 调整

优先处理：

```text
Save As 原文件保护
ExternalChannelMapping
任意 ChannelID 导入
外部数据单位规则
```

确保数据语义稳定。

---

## Phase 2：External Import GUI

实现：

```text
TDMS
MiniSEED
HDF5
Preview
Mapping
Compatibility
Async loading
```

---

## Phase 3：Help 基础设施

实现：

```text
Help menu
User Guide viewer
Format Specification viewer
Homepage
About
```

同时开始建立：

```text
FieldHelpRegistry
FieldLabel
```

但不自行完善完整字段映射文件。

---

## Phase 4：Sensor Layout

实现：

```text
Floor transparent fill
Plan view
X-Z view
Y-Z view
Sensor hover
```

Floor Filter 可根据进度决定是否一起完成。

---

## Phase 5：成熟化

补充：

```text
unit tests
shortcuts
QSettings
Validation navigation
minor UI improvements
```

---

# 33. 本阶段优先级

| 优先级    | 工作                                     |
| ------ | -------------------------------------- |
| **P0** | 禁止 Save As 覆盖原始文件                      |
| **P0** | External Import Mapping 与 ChannelID 解耦 |
| **P1** | TDMS / MiniSEED / HDF5 GUI 导入          |
| **P1** | 外部导入 Preview / Mapping / Compatibility |
| **P1** | 外部数据异步读取                               |
| **P1** | Help 菜单和 User Guide                    |
| **P1** | Field Help 基础设施                        |
| **P1** | Sensor Layout Floor Fill               |
| **P1** | Plan / X-Z / Y-Z Sensor View           |
| **P2** | Sensor hover / floor filter            |
| **P2** | Metadata / Validation / qREST 文件测试     |
| **P2** | 快捷键 / QSettings                        |
| **P3** | GUI/Core qREST I/O 进一步统一               |
| **P3** | Large-file 架构                          |
| **P3** | 真正 3D View                             |

---

# 34. 本阶段验收目标

完成本阶段后，qREST Data Tools 应进一步从：

> 可以完成 qREST 文件编辑的工程工具

发展为：

> **普通工程用户无需理解大量底层协议，也能完成 qREST 文件创建、外部数据导入、检查、验证和维护的正式桌面工具。**

核心验收点包括：

* 能直接导入主要外部数据格式；
* 不再依赖特定 ChannelID 命名进行数据映射；
* External Import 有明确 Preview 和 Mapping；
* 大文件导入过程不冻结 GUI；
* Help 系统可离线使用；
* Metadata 字段具备统一的 Help 扩展机制；
* Sensor Layout 能通过填充和多正交视图更加准确地检查测点位置；
* 原始 qREST 文件无法被 Draft 意外覆盖；
* 已有 qREST 核心文件格式和 Validation 逻辑不被 GUI 重复实现。

Field Help 的**完整映射表内容暂不属于本开发文档范围**。后续在逐字段确认 `label / summary / details / range / example` 后，再单独生成正式 `field_help.json`，届时本阶段只需让 GUI 的 Field Help 基础设施能够直接加载该文件即可。
