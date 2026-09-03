# qREST Data Tools UI 第二轮优化计划

## 1. 本轮目标

第一轮重构已经完成主要架构调整，目前程序已经具备：

- View / Edit Draft / New Draft 文档状态；
- 安全的 Save As 工作流；
- 结构化 Metadata 编辑；
- Channel 编辑；
- Data Table；
- Validation；
- Raw JSON；
- Binary Viewer；
- 初步 Sensor Layout。

本轮不再进行大规模架构推翻，而是在现有：

```text
QrestDocument
    ↓
QrestViewModel
    ↓
Qt Models
    ↓
QML
```

架构基础上进一步完善。

本轮重点：

1. 重构 Sensor Layout；
2. 统一 Validation；
3. 完善 Channel Metadata Schema 和 Channel 编辑逻辑；
4. 优化 Raw JSON 的语义处理；
5. 调整 Building / Channels / Data 页面职责；
6. 拆分和整理 QML；
7. 优化 TableView 和整体界面布局；
8. 优化 Toolbar；
9. 完善 Save As 后的状态逻辑。

---

# 2. ChannelID：UNKNOWN 特殊语义

ChannelID 原则仍然保持：

> ChannelID 由用户自由定义，分析程序不依赖其命名规则。

正常硬件编号应当具有唯一性。

但实际工程中经常无法获得设备硬件编号，因此允许：

```text
UNKNOWN
```

作为明确的未知标记。

## Validation 规则

### 普通 ChannelID

例如：

```text
ACC001
A101
Roof-X
```

要求：

```text
同一 qREST 文件中唯一
```

重复应报 Error。

### UNKNOWN

允许：

```text
Channel 1 = UNKNOWN
Channel 2 = UNKNOWN
Channel 3 = UNKNOWN
```

不进行唯一性检查。

建议 Validation 不为每个 UNKNOWN 产生错误。

可选地只生成一条汇总 Warning：

```text
5 channels use UNKNOWN ChannelID
```

这不是格式错误。

## UI

可以在 ChannelID 输入区增加：

```text
[ Set UNKNOWN ]
```

作为快捷操作。

---

# 3. Channel Metadata Schema：补充 DeviceType

当前 C++ Metadata Schema 漏掉了：

```text
DeviceType
```

本轮需要正式加入。

Channel 最终字段建议：

```text
ChannelNo
ChannelID
DeviceType
Measurand
Scale
Azimuth
LocationXYZ
```

注意：

```text
Direction
```

**不进入 Metadata Schema。**

Direction 始终由：

```text
Azimuth
```

派生。

例如：

```text
-1          → Z
0 / 180     → Y
90 / 270    → X
other angle → HORIZONTAL
```

允许一定角度容差。

DeviceType 的增加必须同步修改：

```text
metadata.hpp
JSON serialization
JSON parsing
Validation
ChannelTableModel
Selected Channel Editor
Add/Duplicate Channel
Raw JSON
```

不要只修改 UI。

---

# 4. Channel 创建逻辑重新明确

目前 Add Channel 与 Duplicate Channel 的语义较接近，需要区分。

## Add Channel

用于：

> 新建一个新的物理通道。

自动继承：

```text
DeviceType
Measurand
Scale
```

不要继承：

```text
ChannelID
Azimuth
LocationXYZ
```

新通道：

```text
ChannelNo    自动生成
ChannelID    空 / UNKNOWN
DeviceType   继承默认值
Measurand    继承默认值
Scale        继承默认值
Azimuth      默认值或待填写
LocationXYZ  待填写
```

---

## Duplicate Channel

用于：

> 同一个测点增加另外一个方向等情况。

完整继承：

```text
DeviceType
Measurand
Scale
Azimuth
LocationXYZ
```

然后：

```text
ChannelNo    自动生成
ChannelID    清空
```

用户主要修改：

```text
ChannelID
Azimuth
```

---

# 5. Channel Defaults

建议显式建立 Channel 默认配置，而不是散落在 Add 代码里。

例如：

```text
ChannelDefaults

DeviceType
Measurand = Acceleration
Scale     = 1.0
```

Add Channel 使用：

```text
ChannelDefaults / LastUsedCommonSettings
```

Duplicate 使用：

```text
Selected Channel
```

两种逻辑保持独立。

---

# 6. Sensor Layout：本轮主要开发目标

当前 Sensor Layout 的主要问题并不是简单绘图错误，而是当前实现本质上仍然是：

```text
XYZ Metadata
    ↓
只使用 XY 绘图
```

不同 Elevation 的建筑轮廓投影到同一个位置，相同 XY 但不同 Z 的 Sensor 也完全重叠。

本轮应重新实现 Sensor Layout。

---

# 7. Sensor Layout 第一版采用 2.5D 线框方案

暂时不强制引入 Qt Quick 3D。

优先实现：

> 固定轴测 / 等角投影视图。

仍然可以使用 Canvas 或自定义 QQuickItem，但必须真正使用：

```text
X
Y
Z
```

三个坐标。

基本过程：

```text
World XYZ
    ↓
projection
    ↓
Screen XY
```

---

# 8. StructureGeometryModel

不要继续让 QML 自己从 Metadata 拼装大量几何逻辑。

建议建立：

```text
StructureGeometryModel
```

或者相应 C++ geometry service。

至少提供：

```text
FloorEdges
VerticalEdges
Sensors
SensorDirections
Axes
Bounds
SelectedSensor
```

QML 主要负责绘制。

---

# 9. 建筑线框

参考 MATLAB `Metadata.m` 中已有思路：

对每个：

```text
Elevation[i]
```

生成完整平面轮廓。

例如 Rectangular：

```text
┌────────────┐
│            │
└────────────┘
```

每层都拥有不同 Z。

然后在关键角点之间生成：

```text
VerticalEdges
```

形成真正的建筑线框。

---

# 10. Shape 支持

Sensor Layout 至少应正确支持：

```text
Rectangular
Circular
Polygon
```

不要只针对 Rectangular 工作。

建筑几何应来源于：

```text
StructuralFootprint
Elevation
```

而不是 BoundingBox 的近似替代。

---

# 11. Sensor 显示

传感器位置：

```text
LocationXYZ
```

必须参与完整 XYZ 投影。

不同高度但相同 XY 的测点，应在画面中正确分离。

Sensor marker 至少表现：

```text
normal
selected
```

两个状态。

---

# 12. Sensor Direction

水平传感器：

```text
ux = sin(Azimuth)
uy = cos(Azimuth)
uz = 0
```

竖向：

```text
Azimuth = -1

ux = 0
uy = 0
uz = 1
```

因此 Geometry Model 不再只提供：

```text
ux
uy
```

而应提供：

```text
ux
uy
uz
```

方向箭头也必须进行同样的 3D → Screen 投影。

---

# 13. Sensor Selection

当前二维 hit-test 需要同步重构。

应采用：

```text
Sensor XYZ
    ↓
Projection
    ↓
Screen Position
    ↓
Mouse Hit Test
```

不能继续只使用 XY。

点击 Sensor 后：

```text
Sensor Layout
     ↓
selectChannel(row)
     ↓
Channel Table
     ↓
Selected Channel Editor
```

保持双向联动。

---

# 14. Geometry Bounds

默认视图范围优先根据：

```text
StructuralFootprint
+
Elevation
```

计算。

不要让远离建筑的 Sensor 自动把整个建筑缩得非常小。

建筑外 Sensor 可以：

- 显示在视图边缘；
- 给出 Geometry Warning；
- 后续增加 `Fit All Sensors`。

默认：

```text
Fit Structure
```

---

# 15. 坐标轴与 North

Sensor Layout 增加：

```text
X
Y
Z
North
```

方向指示。

North 根据：

```text
NorthAngle
```

计算。

这样可以帮助用户理解：

```text
Azimuth
Local Coordinate
Geographical North
```

之间的关系。

---

# 16. Sensor Layout 第一版交互

至少支持：

```text
Select Sensor
Fit Structure
```

如果实现成本较低，可以增加：

```text
Zoom
Pan
```

暂时不要求：

```text
自由 3D Camera rotation
复杂材质
实体建筑
```

第一版重点是：

> 几何正确、信息清晰。

---

# 17. 后续升级路径

如果 2.5D 版本稳定，后续可进一步迁移到：

```text
Qt Quick 3D
```

但 Geometry Model 应保持可复用。

不要让几何计算绑定某种具体绘制技术。

---

# 18. Validation：建立统一代码源

当前 Validation 分散在：

```text
qrest_data_tools validation
QrestDocument
QrestViewModel
```

容易导致 GUI 与 CLI 对同一个文件产生不同结果。

本轮需要统一。

但：

> 不需要建立新的独立项目。

可以直接把通用 Validation 代码放在现有：

```text
qrest_data_tools
```

或者更合适的现有公共模块中。

原则是：

```text
Common Validation Code
        ↓
    ┌───┴────┐
    CLI     GUI
```

---

# 19. Validation 分层

建议区分两类。

## Core / Format Validation

GUI 与 CLI 共用。

检查：

```text
Header
Version
Units
Metadata structure
ElevationNum
ChannelNum
ChannelNo
DeviceType
Measurand
Scale
Azimuth
LocationXYZ
NPTS
DT
StartTime
Packet dimensions
sampling rate
timestamp
data size
checksum / serialization
```

这是 qREST 文件是否合法的核心规则。

---

## UI / Engineering Validation

仅 GUI 使用。

例如：

```text
Sensor outside BoundingBox
Sensor outside elevation range
UNKNOWN ChannelID summary
Incomplete draft
```

这些属于工程体验提示，而不是文件格式错误。

---

# 20. Draft Validation 与 Final Validation

新建文件编辑过程中允许不完整状态。

例如：

```text
没有 Channels
没有 Data
没有 EventName
```

此时显示：

```text
Incomplete
```

而不是不断产生大量阻塞 Error。

---

## Final Validation

Save As 前执行完整校验。

必须满足正式 qREST 文件要求。

```text
Error
    → 阻止保存

Warning
    → 允许保存
```

---

# 21. UNKNOWN Validation

统一规则必须进入共享 Validation policy：

```text
UNKNOWN
```

属于合法的未知 ChannelID。

多个 UNKNOWN 不属于 duplicate ID。

其他 ChannelID 继续执行唯一性检查。

---

# 22. Raw JSON

Raw JSON 的 ScrollView 已手动修复，本轮不再将其作为主要开发任务。

仅保留必要的小型检查：

```text
vertical scrolling
horizontal scrolling
NoWrap
monospace font
```

---

# 23. Raw JSON Apply 逻辑

Raw JSON 虽然是高级入口，但仍应遵守 Metadata 的核心规则。

建议：

```text
Parse JSON
      ↓
Read editable semantic fields
      ↓
Preserve fixed fields
      ↓
Normalize derived fields
      ↓
Validation
      ↓
Apply Draft
```

---

# 24. Fixed Fields

JSON 不允许真正修改：

```text
Header
Version
```

Apply 后由程序恢复为当前 qREST Schema 对应值。

---

# 25. Derived Fields

Raw JSON Apply 后重新计算：

```text
ElevationNum
ChannelNum
ChannelNo
BoundingBox
```

不要让 JSON 编辑器创造：

```text
ChannelNum != Channels.size()
```

这种本可避免的状态。

---

# 26. Metadata 扩展字段

目前 Metadata 强类型解析会丢失 Schema 未声明字段。

本轮建议同时解决这一问题。

推荐：

```text
Typed Core Metadata
+
Extension JSON
```

即：

```text
Known fields
    → 正常 typed model

Unknown fields
    → 原样保留
```

序列化时重新合并。

这样 Raw JSON 才真正具有：

> 高级扩展 Metadata

的意义。

---

# 27. DeviceType 与扩展机制

DeviceType 本轮正式加入 Core Schema。

不要依赖 Extension JSON 表达 DeviceType。

Direction 不加入 Core Schema。

以后未知字段则使用 extension preservation 机制保存。

---

# 28. Units

本轮：

```text
Distance Unit
```

继续允许：

```text
m
cm
mm
```

时间单位暂时固定：

```text
s
```

UI 中 Time Unit：

- 可以只读显示；
- 或直接不提供修改控件。

原因：

Metadata 中：

```text
DT
```

和 Packet：

```text
SamplingRate
```

目前默认都建立在秒制基础上。

暂时避免引入：

```text
ms
```

导致数值和单位语义失配。

后续如果实现完整单位转换，再重新开放。

---

# 29. Data Table 时间显示

当前 Data Table 时间起点：

```text
1 / fs
```

暂时保持现有行为。

本轮不修改。

---

# 30. 页面职责重新整理

当前 Building 页面包含了一些实际上属于 Instrument / Data 的内容。

本轮进行页面迁移。

---

# 31. Building Page

只保留：

```text
Format Summary
Units
Project
Structural Type
GeoLocation
StructuralFootprint
Elevation
Building Geometry Preview（如需要）
```

其中：

```text
Header
Version
```

可以作为轻量只读 Format 信息。

---

# 32. Channels Page

移动：

```text
Provider
```

到 Channels。

最终：

```text
Provider
Channel toolbar
Channel Table
Selected Channel
Sensor Layout
```

---

# 33. Data Page

移动：

```text
EventName
StartTime
SamplingRate
SamplingInterval
NPTS
Corrected
```

到 Data。

再加：

```text
Import Data
Export Data
Data Table
```

这样普通用户面对的就是完整的：

> Data Information + Dataset

---

# 34. Packet Header

以下内容：

```text
SourceID
Encoding
Packet ChannelCount
Packet DataPointCount
Packet Timestamp
```

逐步从普通 Data 页面弱化。

建议最终进入：

```text
Advanced
    → Packet Inspector
```

普通用户无需直接修改大部分 Packet Header。

---

# 35. Save As 状态

当前 Save As 后文档仍可能保持：

```text
Editing Copy
```

或：

```text
New Draft
```

本轮建议修改为：

```text
Save As
    ↓
写入新文件
    ↓
当前文件切换为保存后的文件
    ↓
View / Read Only
```

即：

```text
Draft
  ↓ Save As
Saved File
  ↓
Read Only
```

如需继续修改：

```text
Edit
```

重新创建 Draft。

这样完整贯彻：

> 已保存文件默认只读。

---

# 36. QML 文件拆分

当前 `main.qml` 已经超过适合持续维护的体量。

本轮建议拆分。

例如：

```text
main.qml

pages/
    OverviewPage.qml
    BuildingPage.qml
    ChannelsPage.qml
    DataPage.qml
    ValidationPage.qml

components/
    ChannelTable.qml
    ChannelEditor.qml
    SensorLayoutView.qml
    DataTable.qml
    ValidationTable.qml

dialogs/
    RawMetadataDialog.qml
    BinaryViewerDialog.qml
    PacketInspectorDialog.qml
```

不要为了“一次拆完”导致大量不必要重构。

优先拆：

```text
SensorLayoutView
ChannelsPage
DataPage
ValidationPage
```

因为这些是下一轮仍会频繁修改的部分。

---

# 37. Channels 左右布局比例

当前 Channels 的 SplitView 使用：

```text
SplitView.preferredWidth
```

绝对像素尺寸。

在不同窗口宽度下容易产生比例不合理。

改为：

> 左侧 Channel 区域明显大于右侧属性/几何区域，并随窗口动态变化。

建议初始比例：

```text
Channel List        60~65%
Selected/Sensor     35~40%
```

例如目标：

```text
┌───────────────────────────────┬───────────────────┐
│                               │                   │
│         Channel List          │ Selected Channel  │
│                               │                   │
│           ~ 65%               │      ~ 35%        │
│                               │ Sensor Layout     │
└───────────────────────────────┴───────────────────┘
```

仍允许用户拖动 SplitHandle 调整。

不要完全固定像素宽度。

---

# 38. TableView 表头 / 首列遮挡问题

Channels、Data、Validation 页面目前都应统一检查。

当前采用：

```text
corner
HorizontalHeaderView
VerticalHeaderView
TableView
```

组合方案本身没问题。

但必须保证四个区域拥有明确且互不重叠的布局：

```text
┌────────┬──────────────────────┐
│ corner │ HorizontalHeaderView │
├────────┼──────────────────────┤
│ Vert.  │                      │
│ Header │      TableView       │
│        │                      │
└────────┴──────────────────────┘
```

---

# 39. Table Layout 要显式指定 Grid 位置

不要依赖 GridLayout 的隐式排列。

明确：

```text
corner
    Layout.row: 0
    Layout.column: 0

HorizontalHeaderView
    Layout.row: 0
    Layout.column: 1

VerticalHeaderView
    Layout.row: 1
    Layout.column: 0

TableView
    Layout.row: 1
    Layout.column: 1
```

这样可以避免页面调整后元素进入错误 Grid cell。

---

# 40. Header 尺寸同步

重点检查：

```text
HorizontalHeaderView
```

和：

```text
TableView.columnWidthProvider
```

是否存在宽度冲突。

目前 Channel Table 的列宽并不统一，例如 ChannelID 等列拥有不同宽度，而 Header delegate 又有自己的 implicitWidth。

应保证：

> Header 使用 TableView 实际 column width。

不要让：

```text
Header width = 110
Table column width = 180
```

产生错位。

---

# 41. Row Height 同样统一

设置明确统一的：

```text
rowHeightProvider
```

或统一 delegate implicitHeight。

Horizontal Header、Vertical Header 与 Table body 不应各自使用不同高度规则。

---

# 42. Clip / Z Order

所有 Header 和 Table：

```text
clip: true
```

Header：

```text
z > table
```

但 Header 不应覆盖 Table 的实际 viewport。

如果当前出现：

> 首行 / 首列遮挡数据

需要重点排查：

```text
anchors.fill
Layout.fill*
z
implicit size
syncView
```

是否混用导致几何区域重叠。

---

# 43. 统一 Table 组件

由于：

```text
Channels
Data
Validation
Binary
```

都在使用相似 TableView + HeaderView 结构，本轮建议抽出公共组件。

例如：

```text
QrestTableView.qml
```

负责：

```text
corner
horizontal header
vertical header
TableView
scrollbars
selection colors
row colors
```

页面只传入：

```text
model
columnWidthProvider
selectionModel
```

这样修复一次表头问题即可作用于所有页面。

---

# 44. Table 第一列语义

如果 Table Model 本身已经有：

```text
ChannelNo
```

同时 VerticalHeader 又显示：

```text
row number
```

注意不要产生视觉上两个连续的：

```text
#
No
```

导致用户误解。

Channels 可以考虑：

```text
VerticalHeader = UI row
ChannelNo      = qREST channel order
```

但两者视觉上需有明显区别。

或者简化其中一项。

---

# 45. 页面总体布局优化

本轮不需要重新设计视觉语言，但应统一以下原则。

## 页面标题

统一：

```text
Page Title
short summary / state
```

例如：

```text
Channels
24 configured channels
```

---

## GroupBox 使用

避免大量小 GroupBox 嵌套。

只用于真正具有语义边界的区域：

```text
Structural Footprint
Selected Channel
Sensor Layout
```

普通字段可以使用轻量 section title。

---

## 间距

统一：

```text
page margin
section spacing
field spacing
```

避免不同页面：

```text
10
12
15
20
```

随机混用。

建议建立一组统一常量。

---

# 46. 页面可滚动性

Building 等表单页面内容较长。

统一使用：

```text
ScrollView
```

包裹表单主体。

窗口缩小时仍应能够访问所有字段。

不要依赖增大 Window 才能看到底部内容。

---

# 47. Toolbar 优化

当前主要操作使用纯文字 Button：

```text
New
Open
Edit
Validate
Save As
```

本轮改为：

```text
ToolBar
+
ToolButton
```

---

# 48. Icon 资源

用户后续提供图标资源。

推荐使用：

```text
SVG
```

优先。

资源组织例如：

```text
resources/icons/

new.svg
open.svg
edit.svg
validate.svg
save_as.svg
json.svg
binary.svg
```

加入 qrc。

---

# 49. ToolButton 设计

每个按钮：

```text
icon
+
short text
```

例如：

```text
[icon] New
[icon] Open
[icon] Edit
[icon] Validate
[icon] Save As
```

窗口较窄时未来可以只显示 Icon。

---

# 50. Tooltip

所有 Toolbar 图标必须有：

```text
ToolTip
```

例如：

```text
Create new qREST file
Open qREST file
Create editable copy
Validate current document
Save draft as a new file
```

不要让纯 Icon 成为不可理解操作。

---

# 51. Toolbar 分组

推荐：

```text
File
    New
    Open
    Save As

Edit
    Edit Draft

Validation
    Validate

Advanced
    JSON
    Binary
```

可以使用 Separator 做视觉分组。

---

# 52. 状态表达

Toolbar 中 Edit / Save As 根据 Document Mode 自动 enabled。

不要通过弹错误信息告诉用户：

```text
当前操作不可用
```

能预先禁用的就直接禁用。

---

# 53. 其他小型修复

本轮可以顺带处理：

### Raw JSON

ScrollView 已完成，只做功能验证。

### Binary Viewer

确认：

```text
horizontal scroll
vertical scroll
offset jump
search
```

在较大文件下正常。

### Channel Editor

为：

```text
Azimuth
Scale
XYZ
```

增加更清晰的单位/范围提示。

### Validation

Validation 页面应增加：

```text
Errors
Warnings
Info
```

视觉区分。

后续再实现 issue → field navigation。

---

# 54. 本轮优先级

## P0：核心功能

### Sensor Layout

- 2.5D XYZ projection；
- building wireframe；
- vertical edges；
- sensor XYZ；
- sensor arrows；
- Z direction；
- selection；
- axes / North。

### Validation

- 共用 Core Validation；
- Draft / Final Validation；
- UNKNOWN 特殊规则；
- CLI / GUI 一致。

### Metadata Schema

- 加入 DeviceType；
- Direction 保持派生；
- extension preservation。

---

## P1：Channel 与页面逻辑

- Add / Duplicate 分离；
- Channel Defaults；
- Building / Channels / Data 信息迁移；
- Save As → Read Only；
- Time Unit 固定为 s；
- Raw JSON derived-field normalization。

---

## P1：UI 稳定性

- Channels SplitView 改为相对比例；
- 修复 Channels/Data/Validation Table header overlap；
- 抽取统一 Table 组件；
- 页面 ScrollView；
- 布局 spacing/margins 统一。

---

## P2：代码维护性

- 拆 main.qml；
- SensorLayoutView.qml；
- ChannelsPage.qml；
- DataPage.qml；
- ValidationPage.qml；
- Advanced dialogs 分离。

---

## P2：视觉优化

- Toolbar ToolButton；
- Icon 资源；
- Tooltip；
- Separator；
- 页面标题与 section 样式统一；
- Validation severity 风格。

---

# 55. 本轮实现原则

本轮已经进入成熟化阶段。

优先级应当是：

```text
数据语义正确
    ↓
文件安全
    ↓
几何显示正确
    ↓
UI 逻辑一致
    ↓
代码可维护
    ↓
视觉美化
```

不要为了快速改善视觉效果，把 Validation、Metadata 或 Geometry 逻辑再次放回 QML。

尤其：

```text
Channel rules
Validation rules
Metadata normalization
Geometry generation
```

尽量保留在 C++。

QML 负责：

```text
layout
input
interaction
rendering
```

---

# 56. 本轮预期结果

完成后，qREST Data Tools 应达到：

### Metadata

可以可靠创建和修改正式 qREST Metadata。

### Channels

支持：

```text
DeviceType
UNKNOWN ChannelID
Add
Duplicate
derived Direction
```

且逻辑明确。

### Geometry

Sensor Layout 能够真正表现：

```text
建筑高度
各层轮廓
传感器 XYZ
传感器方向
```

而不再只是二维符号。

### Validation

CLI 和 GUI 使用同一核心规则。

### UI

Building / Channels / Data 职责清楚；

表格不再发生 Header 遮挡；

窗口缩放时布局保持稳定；

Toolbar 更接近正常桌面工程软件。

### Architecture

`main.qml` 不再承担整个程序所有页面和高级 Dialog 的实现。

整体进入：

> 可持续继续扩展，而不是继续堆积单文件逻辑

的状态。