# qREST Data Tools UI 重构开发计划

## 1. 项目目标

本次修改针对 `qrest_data_tools_gui` 进行较大范围重构。

当前程序主要以 qREST 文件内部二进制结构为中心，按照 File Header、Metadata、Data Packet 等部分直接展示文件内容，更接近“qREST 文件格式查看器/调试器”。

新版程序的目标是将其重构为：

> **面向普通用户的 qREST 数据创建、查看、修改、校验与可视化工具，同时保留面向高级用户的 JSON 和二进制底层查看能力。**

主要能力包括：

- 新建 qREST 数据文件；
- 打开并查看已有 qREST 文件；
- 以安全的 Draft 方式修改已有文件；
- 结构化编辑 Metadata；
- 查看和配置 Channels；
- 以简单建筑线框图可视化建筑和传感器布置；
- 查看数据表格；
- 对 Metadata、Packet 和数据一致性进行校验；
- 将编辑结果另存为新的 qREST 文件；
- 高级用户仍可查看/编辑 Raw Metadata JSON；
- 高级用户可查看完整 qREST 文件的二进制内容。

本次属于较大规模的 UI 和应用层重构。

**不要求保留旧 UI 的代码结构或页面组织方式。**

如现有代码会明显限制新版设计，可以直接废弃并重新实现。仅保留以下类型的旧代码：

- 已经成熟且职责明确的 qREST 数据读写逻辑；
- 可直接复用的数据模型；
- 与新架构一致的工具函数；
- 能明显降低重复实现成本的组件。

不要为了兼容旧 UI 而增加不必要的适配层。

---

# 2. 总体设计原则

## 2.1 用户界面面向工程概念，而不是文件格式字段

普通用户主要应该看到：

- Project；
- Building；
- Channels；
- Data；
- Validation。

不应该要求普通用户理解：

- MetadataSize；
- Packet Header；
- BodySize；
- Raw JSON；
- 二进制 offset；
- CRC32。

这些内容仍然保留，但放入高级工具中。

---

## 2.2 查看、编辑、新建共享同一个 Workspace

不要为：

- 查看文件；
- 新建文件；
- 修改文件；

分别维护三套 UI。

统一进入同一个 Workspace，只根据 Document State 改变控件权限。

建议定义三种文档状态：

### View Mode

来源：

```text
Open existing qREST file
```

行为：

- 所有业务字段只读；
- 原文件永远不允许直接覆盖；
- 用户可以查看所有内容；
- 用户可以主动点击 `Edit`；
- 用户第一次尝试修改字段时，也可以提示进入编辑模式。

---

### Edit Draft

来源：

```text
Existing qREST
        ↓
Create editable copy
```

行为：

- 在内存 Draft 上编辑；
- 原始文件始终保持不变；
- 保存只能执行 `Save As`；
- 显示 dirty / unsaved 状态；
- 最终保存前执行完整校验。

---

### New Draft

来源：

```text
New File
```

行为：

- 创建新的 Metadata + Data Draft；
- 用户逐步填写必要字段；
- 程序补充派生字段；
- 校验通过后保存为新的 qREST 文件。

---

# 3. 文件安全策略

这是本次重构必须严格遵守的原则。

## 3.1 打开的 qREST 文件默认只读

打开已有文件后：

```text
Read Only
```

不得直接修改原始文件。

顶部可显示：

```text
filename.qrest       Read Only       [Edit]
```

---

## 3.2 编辑已有文件时创建 Draft

点击 Edit 后：

```text
Original File
     ↓
Editable Draft
```

所有修改只应用于 Draft。

不立即写入磁盘。

---

## 3.3 已有文件禁止直接覆盖保存

已有文件的修改只能：

```text
Save As...
```

不能提供直接覆盖原文件的普通 Save 操作。

---

## 3.4 新建文件可以正常保存

New Draft 第一次保存时：

```text
Save As...
```

保存完成以后可以根据需要继续采用 Draft 工作方式。

---

## 3.5 Dirty State

任何修改后：

```text
Editing Copy *
```

或：

```text
Unsaved Changes
```

关闭文件、打开其他文件或退出程序时，应检查 dirty 状态。

---

# 4. 新建文件流程

新建文件不建议直接展示一个包含全部 Metadata 字段的大表单。

先使用一个轻量初始化界面。

例如：

```text
Create qREST File

Project Name
Structural Type
Distance Unit
Time Unit
Sampling Rate

Data:
    Import now
    Configure later
```

用户完成最基本信息后进入正常 Workspace。

后续继续填写：

- Building；
- Channels；
- Data。

不要开发复杂的多步骤 Wizard，避免新建流程过于繁琐。

---

# 5. 主界面信息架构

旧版：

```text
Part 0 Header
Part 1 Metadata
Part 2 Data Packet
```

整体废弃。

新版左侧主导航建议为：

```text
Overview
Building
Channels
Data
Validation
```

高级功能不进入主导航。

---

# 6. 主工具栏

建议包含：

```text
New
Open
Edit
Validate
Save As
```

视当前状态自动启用/禁用。

例如 View Mode：

```text
New
Open
Edit
Validate
```

Edit Draft：

```text
New
Open
Validate
Save As
```

---

# 7. 高级工具

高级接口放入菜单或工具栏，而不是主 Workspace。

建议：

```text
Advanced
    Raw Metadata JSON
    Binary Viewer
    Header / Packet Inspector
```

---

# 8. Raw Metadata JSON

Raw JSON 必须保留。

定位：

> 面向熟悉 qREST 格式的高级用户，提供 Metadata 完整 JSON 的查看和高级编辑能力。

建议能力：

- 查看完整 Metadata JSON；
- JSON 格式化；
- JSON 语法检查；
- Raw JSON 编辑；
- Apply to Draft；
- Import JSON；
- Export JSON。

普通结构化 Metadata 页面和 Raw JSON 应操作同一个 Draft。

Raw JSON 不应该成为普通用户的主要 Metadata 编辑入口。

需要特别注意：

如果 qREST Metadata 规范允许自定义扩展字段，则新的 Metadata 数据模型不能在 JSON → typed struct → JSON 的往返过程中静默丢失未知字段。

建议后续考虑：

```text
Typed Core Metadata
+
Preserved Extension JSON
```

或者：

```text
Canonical JSON Object
+
Typed Access Layer
```

---

# 9. Binary Viewer

当前只显示部分二进制内容的方案废弃。

新版 Binary Viewer 应能够查看：

> **完整 qREST 文件的所有字节。**

Binary Viewer 只读，不允许修改。

建议界面：

```text
Offset      Hex Bytes                         ASCII

00000000    71 52 45 53 54 00 ...            qREST...
00000010    ...
```

支持：

- 完整文件显示；
- Offset；
- Hex；
- ASCII；
- 跳转 offset；
- 搜索 ASCII；
- 搜索 byte sequence；
- 跳转 File Header；
- 跳转 Metadata；
- 跳转 Packet Header；
- 跳转 Packet Body。

可以用不同区域标记：

```text
File Header
Metadata
Packet Header
Packet Body
```

Binary Viewer 必须使用高效的分页/虚拟加载，不要把大型文件一次性转换成超长 QString。

---

# 10. Metadata 字段职责分类

Metadata 字段应分为：

1. 固定字段；
2. 默认可编辑字段；
3. 用户输入字段；
4. 程序派生字段；
5. 数据联动字段。

---

# 11. 顶层 Metadata

## Header

```text
qREST_DATA
```

- 程序固定；
- 只读显示；
- 不允许编辑。

---

## Version

- 与当前 qREST 格式版本匹配；
- 程序生成；
- 只读显示。

不要让用户自行输入 Metadata Version。

---

## Units

默认：

```text
Distance: m
Time: s
```

允许用户修改。

UI 使用下拉框，不使用任意字符串。

例如：

```text
Distance Unit    [m ▼]
Time Unit        [s ▼]
```

所有长度相关控件旁同步显示当前单位。

例如：

```text
Length       42.0 m
Elevation    15.0 m
Location X   3.2 m
```

如果用户在已有 Metadata 中修改 Units，需要明确决定：

```text
仅修改单位声明
```

还是：

```text
同时转换已有数值
```

不能静默改变语义。

---

# 12. Building 页面

主要包含：

```text
Basic Information
Geo Location
Structural Footprint
Elevation
Geometry Preview
```

---

# 13. BuildingInfo.Basic

## ProjectName

用户输入。

---

## StructuralType

使用下拉选择。

例如：

```text
RC Frame
Shear Wall
Steel Frame
Masonry
Mixed Structure
Other
```

内部映射成统一的规范字符串。

选择 `Other` 时允许自定义。

不要允许同一种结构类型因为自由字符串输入产生大量不同名称。

---

# 14. GeoLocation

用户输入：

```text
Longitude
Latitude
NorthAngle
```

使用数值输入并进行范围检查。

建议：

```text
Longitude   [-180,180]
Latitude    [-90,90]
NorthAngle  [0,360)
```

NorthAngle 需要在 UI 中提供简短说明。

---

# 15. StructuralFootprint

## Shape

使用下拉：

```text
Rectangular
Circular
Polygon
```

不同 Shape 动态加载不同 Parameter Editor。

---

## Rectangular

显示：

```text
Length
Width
```

---

## Circular

显示：

```text
Radius
```

---

## Polygon

显示：

```text
Corners
```

可以采用坐标表格：

```text
No   X   Y
```

允许增删顶点。

---

# 16. BoundingBox

BoundingBox 不允许用户输入。

完全由 Shape + Parameters 自动计算。

UI 只读显示：

```text
Bounding Box

X: min ~ max
Y: min ~ max

Calculated automatically
```

---

# 17. Elevation

Elevation 由用户输入。

不建议逐项点击：

```text
+ Add Elevation
```

采用数组文本输入更方便。

支持以下分隔形式：

```text
0, 3.6, 7.2, 10.8
```

```text
0 3.6 7.2 10.8
```

```text
0
3.6
7.2
10.8
```

支持：

- 逗号；
- 空格；
- 分号；
- 换行。

解析后显示结果预览。

例如：

```text
Elevation

0, 3.6, 7.2, 10.8

Parsed: 4 levels
```

---

## ElevationNum

程序生成：

```text
ElevationNum = Elevation.size()
```

只读显示。

---

## Elevation Validation

至少检查：

- 是否可以全部解析为数字；
- 是否存在重复值；
- 是否严格递增。

不要自动排序错误输入。

---

# 18. InstrumentInfo

主要包含：

```text
Provider
Channels
```

---

## Provider

用户输入。

---

## ChannelNum

程序生成：

```text
ChannelNum = Channels.size()
```

只读展示。

---

# 19. Channels 页面总体设计

Channels 页面是 Metadata 编辑的核心。

建议采用：

```text
Channel Table
+
Selected Channel Editor
+
Sensor Layout
```

而不是一个非常宽的 Excel 风格表格。

---

# 20. Channel 数据结构职责

建议 Channel 包含：

```text
ChannelNo
ChannelID
DeviceType
Measurand
Scale
Azimuth
Direction
LocationXYZ
```

其中需要注意：

当前正式 C++ Metadata Schema 是否包含 `DeviceType` 和 `Direction`，需要与 qREST Metadata 规范同步。

如果新增字段，必须修改：

- Metadata schema；
- serializer；
- parser；
- validation；
- JSON；
- UI；

不要只在 GUI 中加入字段。

---

# 21. ChannelNo

完全由程序生成。

```text
ChannelNo = index + 1
```

UI 只读。

删除通道后重新连续编号。

---

# 22. ChannelID

ChannelID 完全允许用户自定义。

不强制：

```text
X1
Y1
Z1
```

也不要求 ChannelID 包含：

- 楼层；
- 方向；
- DeviceType。

用户可以按照自己的工程习惯命名。

例如：

```text
Roof-East
B01
ACC-101
RF_X
```

建议 Validation：

- ChannelID 不允许为空；
- 同一文件中 ChannelID 应唯一。

ChannelID 不参与决定 Packet 数据顺序。

---

# 23. Channel 数据顺序

必须区分：

```text
显示顺序
```

和：

```text
qREST Packet 中的物理通道顺序
```

Data Packet Body 与 Channels[] 的顺序必须保持严格对应。

因此：

## 尚未导入数据

可以允许：

- Move Up；
- Move Down；
- 拖动调整真实通道顺序。

程序重新生成 ChannelNo。

## 已导入数据

第一版建议锁定真实通道顺序。

表格可以支持：

```text
Sort by ID
Sort by Elevation
Sort by Direction
Sort by Device
```

但是这些只改变 View 排序，不修改 Packet Body 中实际通道顺序。

如果未来实现真实通道重排，则必须同步重排完整 Packet Body。

---

# 24. DeviceType

如果正式进入 qREST Metadata，则允许每个 Channel 独立配置。

UI 建议支持默认值。

例如：

```text
Default Channel Settings

DeviceType
Measurand
Scale
```

整栋建筑通常设备类型一致，但仍允许个别 Channel 覆盖。

---

# 25. Measurand

推荐下拉：

```text
Acceleration
Velocity
Displacement
Strain
Temperature
Other
```

选择 Other 时允许自定义。

避免产生：

```text
Acceleration
acc
Accel
ACC
```

等无法统一解释的字符串。

---

# 26. Scale

用户输入数值。

支持：

```text
Default Scale
```

新建 Channel 默认继承。

需要在 UI tooltip 中说明 Scale 的数据语义，避免与设备灵敏度等概念混淆。

---

# 27. Azimuth 和 Direction

Azimuth 是真实输入。

Direction 是程序根据 Azimuth 派生出的解释结果。

不要同时把两者都作为独立输入。

建议：

```text
Orientation

Horizontal
Vertical
```

Horizontal：

```text
Azimuth     [90] °
Direction   X        Auto
```

Vertical：

```text
Direction   Z        Auto
```

内部可以继续使用协议约定：

```text
Azimuth = -1
```

表示竖向。

但 UI 不应该要求用户知道 `-1` 的含义。

---

# 28. Channel Direction Classification

可以参考 MATLAB 版本中的逻辑：

```text
Azimuth ≈ 90 / 270    → X
Azimuth ≈ 0 / 180     → Y
Azimuth = -1          → Z
Other finite azimuth  → HORIZONTAL
```

具体角度容差应集中定义，不要散落在 QML 中。

建议在 C++ domain/application 层实现：

```text
classifyChannelDirection()
```

---

# 29. LocationXYZ

使用三个独立数值控件：

```text
Location

X
Y
Z
```

不要要求用户输入：

```text
[x,y,z]
```

数组字符串。

位置改变后几何视图实时更新。

---

# 30. Add Channel

新增 Channel 时：

```text
ChannelNo      auto
ChannelID      empty
DeviceType     default / last used
Measurand      default / last used
Scale          default / last used
Azimuth        empty
LocationXYZ    empty
```

---

# 31. Duplicate Channel

另提供：

```text
Duplicate Channel
```

用于同一物理位置不同测量方向等场景。

复制：

```text
DeviceType
Measurand
Scale
LocationXYZ
Azimuth
```

然后：

```text
ChannelNo   自动重新生成
ChannelID   清空并要求重新输入
```

这样非常适合同一位置 X/Y/Z 多通道创建。

---

# 32. Channel Table

默认只展示最有辨识度的字段：

```text
No.
Channel ID
Device Type
Measurand
Direction
Elevation / Z
```

详细字段：

```text
Scale
Azimuth
X
Y
Z
```

放到 Selected Channel Editor。

后续可以支持：

```text
Visible Columns...
```

允许高级用户自定义表格列。

---

# 33. Channel Group / Filter

表格可以支持显示分组：

```text
None
Elevation
DeviceType
Measurand
Direction
```

其中建筑监测场景优先支持：

```text
Group by Elevation
```

可以参考 MATLAB 中：

```text
getInstrumentHeights()
getChannelsAtHeight()
```

的思想。

高度相近的 Channel 可以使用小容差进行分组。

注意：

> 分组和排序只影响 UI，不影响实际 Packet channel order。

---

# 34. Selected Channel Editor

点击一个 Channel 后，在独立区域编辑：

```text
Channel ID
DeviceType
Measurand
Scale

Orientation
Azimuth
Direction [Auto]

Location
X
Y
Z
```

这样避免在超宽表格中编辑所有数据。

---

# 35. 几何可视化总体目标

Metadata 不应该只作为字段集合展示。

需要建立：

> Building Geometry + Sensor Layout

可视化。

目标不是建立高精度 BIM 模型，而是提供一个轻量、准确、可交互的工程线框视图。

---

# 36. MATLAB 参考代码说明

用户提供的 MATLAB `Metadata.m` 可以作为几何计算和逻辑设计参考。

主要参考思想：

### 建筑轮廓

根据：

```text
StructuralFootprint
BoundingBox
Elevation
```

绘制每个标高层的平面轮廓。

---

### 建筑竖向边界

连接底部和顶部对应角点，形成简单 3D 线框。

---

### 传感器位置

使用：

```text
LocationXYZ
```

绘制传感器 marker。

---

### 方向箭头

根据：

```text
u = sin(Azimuth)
v = cos(Azimuth)
```

计算平面方向。

qREST 当前约定：

```text
0°   → +Y
90°  → +X
```

---

### 高度组织

MATLAB 中已有：

```text
getInstrumentHeights()
getChannelsAtHeight()
```

等逻辑。

可以作为 Qt Channels 分组和视图过滤的参考。

---

### Direction Classification

MATLAB 中已有：

```text
classifyChannelDirection()
```

相关逻辑。

建议迁移思想，而不是简单翻译 MATLAB 代码。

---

# 37. Qt 几何架构

不要把 MATLAB 的：

```text
plotStructure()
```

直接翻译成一个大型 Qt 绘制函数。

推荐：

```text
Metadata / Channel Model
          ↓
StructureGeometryModel
          ↓
Geometry View
```

StructureGeometryModel 提供：

```text
floor outlines
vertical edges
sensor positions
sensor directions
selected channel
bounding box
elevations
```

View 只负责绘制。

---

# 38. Geometry Views

建议支持：

```text
3D
Plan
Elevation
```

---

## 3D View

主要用于：

- 整体建筑理解；
- 各高度测点分布；
- 建筑线框；
- 传感器位置；
- 传感器方向。

可以优先采用 Qt Quick 3D。

第一版只需要：

- line geometry；
- sensor marker；
- arrow；
- camera；
- rotate；
- zoom；
- fit view。

不需要复杂实体和材质。

---

## Plan View

用于精确查看：

```text
X
Y
Azimuth
Structural Footprint
North Angle
```

显示：

- 建筑平面轮廓；
- Local X/Y；
- North Arrow；
- Sensor markers；
- Sensor direction arrows。

---

## Elevation View

用于：

```text
Z
Elevation[]
Sensor heights
```

显示建筑各标高和传感器高度。

---

# 39. Geometry 与 Channel 联动

必须实现双向选择。

### Table → Geometry

点击 Channel 行：

```text
Select Channel
      ↓
Highlight sensor
```

---

### Geometry → Table

点击传感器：

```text
Select sensor
      ↓
Select corresponding Channel row
      ↓
Open Selected Channel Editor
```

---

### Property → Geometry

修改：

```text
LocationXYZ
Azimuth
```

几何图实时刷新。

---

# 40. Geometry Validation

几何图同时可以帮助 Validation。

例如：

```text
Sensor outside structural footprint
```

建议 Warning。

```text
Sensor Z outside elevation range
```

建议 Warning。

不要直接 Error，因为可能存在：

- free-field sensor；
- basement sensor；
- external equipment；
- adjacent structure sensor。

---

# 41. DataInfo

Data 页面主要编辑：

```text
EventName
StartTime
Sampling
NPTS
Corrected
```

---

# 42. EventName

用户输入。

---

# 43. StartTime

使用日期 + 时间编辑器。

UI 不要求直接输入 ISO8601。

内部统一转换成规范格式。

---

# 44. Sampling

UI 优先让用户输入：

```text
Sampling Rate
```

例如：

```text
100 Hz
```

程序自动计算：

```text
DT = 1 / SamplingRate
```

Metadata 中保存 DT。

Packet 中保存 SamplingRate。

界面同时显示：

```text
Sampling Rate       100 Hz
Sampling Interval   0.01 s    Auto
```

只维护一个用户输入真值。

---

# 45. NPTS

新建且尚无数据时，可以临时由用户配置。

一旦数据导入：

```text
NPTS = actual sample count
```

之后只读。

如预设值与导入数据不一致，应弹出明确选择：

```text
Cancel Import
Use Imported Value
```

不要静默修改。

---

# 46. Corrected

不建议自由字符串。

使用规范化下拉选择，例如：

```text
Raw / Uncorrected
Corrected
Unknown
```

具体内部字符串需要与 qREST Metadata 规范统一。

---

# 47. Data 页面

本次改版暂时仍以表格显示数据。

可以包含：

```text
Data Information
Data Table
```

暂不要求立即实现复杂波形绘图。

但 DataTableModel 的设计应考虑未来扩展波形浏览。

大数据量时不要把全部数据一次性转换为大量 QML 对象。

---

# 48. Validation 页面

Validation 是正式功能，而不是保存失败时的提示。

需要区分：

```text
Error
Warning
```

显示：

```text
✓ Building geometry valid
✓ 18 channels configured
⚠ Channel RF-Z outside footprint
✕ Duplicate ChannelID: ACC01
```

支持点击问题后：

```text
Navigate to relevant page / field
```

左侧导航可以显示计数：

```text
Channels      ⚠ 2
Validation    ✕ 1
```

---

# 49. Validation 与编辑实时联动

不必每次输入一个字符都执行全部重型校验。

可以分：

### Lightweight Validation

字段修改后立即执行：

- 数值范围；
- required；
- duplicate ChannelID；
- Elevation；
- Shape parameters；
- Channel position。

### Full Validation

点击：

```text
Validate
```

或：

```text
Save As
```

时执行：

- Metadata；
- Packet；
- Data；
- Size；
- Sampling；
- Timestamp；
- Channel count；
- NPTS；
- checksum / serialization consistency。

---

# 50. Document/Application 架构

建议在现有 `QrestViewModel` 之下或替代其部分职责，引入明确的 Document 层。

例如：

```text
qrest_data_lib
        ↓
qrest application/tool services
        ↓
QrestDocument
        ↓
ViewModels / Models
        ↓
QML
```

---

# 51. QrestDocument 建议职责

至少包含：

```text
DocumentMode
CurrentSourcePath
DraftMetadata
DraftPacket
DirtyState
ValidationReport
OriginalSnapshot
```

以及：

```text
open()
newDocument()
beginEdit()
saveAs()
validate()
importData()
exportData()
```

---

# 52. 不要在 QML 中实现 Domain Logic

以下内容应放 C++：

- Metadata parsing；
- Metadata serialization；
- Channel direction classification；
- BoundingBox calculation；
- Elevation parsing；
- Channel numbering；
- Packet/Data consistency；
- validation；
- sampling conversions；
- timestamp conversions；
- geometry generation；
- binary offset calculations。

QML 只负责：

- 页面布局；
- 用户输入；
- 数据绑定；
- 交互；
- 绘制。

---

# 53. 推荐专用 Model

可以考虑：

```text
QrestDocument
ChannelTableModel
DataTableModel
ValidationModel
StructureGeometryModel
```

必要时再拆：

```text
BuildingViewModel
DataInfoViewModel
```

不要求一开始过度细分。

原则：

> 避免重新形成一个包含全部业务逻辑的超大型 QrestViewModel。

---

# 54. 默认值体系

建议建立正式的默认值模型。

例如：

```text
DocumentDefaults

Units        m / s
Shape        Rectangular
Measurand    Acceleration
Scale        1.0
Encoding     Float32
```

Channel 另外维护：

```text
ChannelDefaults
```

或：

```text
LastUsedChannelSettings
```

避免在多个 QML 按钮中手工复制字段。

---

# 55. 派生字段统一原则

以下字段不允许用户直接维护：

```text
ElevationNum
ChannelNum
ChannelNo
BoundingBox
Direction
DT / SamplingRate 中的派生一方
Data imported 后的 NPTS
```

原则：

> 能由唯一来源可靠计算出来的字段，不让用户同时维护第二份值。

Validation 不应该用于补救本可以通过数据模型彻底消除的不一致。

---

# 56. 旧代码处理原则

本次重构允许大规模删除旧代码。

不要：

```text
为了保留旧页面
    ↓
不断增加 if / adapter / compatibility code
```

推荐直接替换旧 UI。

---

## 建议保留

### `qrest_data_lib`

继续作为 qREST：

- serialization；
- parsing；
- binary format；
- checksum；

的底层权威实现。

---

### DataTableModel

如果结构合适，可以重构后继续使用。

不要为了保留类名而保留不合理实现。

---

### 可复用工具逻辑

例如：

- timestamp conversion；
- encoding conversion；
- format utilities。

---

## 建议废弃/重写

### 当前 Part 0 / Part 1 / Part 2 页面

整体废弃。

---

### 当前 Raw JSON 主页面

移动到 Advanced。

---

### 当前部分 Hex Preview

废弃，改为完整 Binary Viewer。

---

### QrestViewModel 中大量文件处理和 UI 逻辑混合的部分

逐步转移至 QrestDocument / application services。

---

# 57. 建议开发阶段

## Phase 1：Document 架构

优先完成：

- DocumentMode；
- View / Edit Draft / New Draft；
- Dirty；
- Open；
- Edit；
- Save As；
- unsaved protection。

此阶段不用追求漂亮 UI。

---

## Phase 2：Metadata Structured Editor

完成：

- Header / Version；
- Units；
- BuildingInfo；
- Footprint；
- Elevation；
- DataInfo；
- derived fields。

---

## Phase 3：Channels

完成：

- ChannelTableModel；
- Add；
- Duplicate；
- Delete；
- Channel defaults；
- ChannelID；
- DeviceType；
- Measurand；
- Scale；
- Azimuth；
- Direction；
- LocationXYZ；
- ChannelNum / ChannelNo 自动维护。

---

## Phase 4：Geometry Visualization

基于 MATLAB 逻辑重新实现：

- StructureGeometryModel；
- floor outlines；
- vertical edges；
- sensor markers；
- arrows；
- 3D；
- Plan；
- selection linkage。

---

## Phase 5：Validation

复用现有 validation 能力，并补充：

- UI field validation；
- geometry warning；
- duplicate ChannelID；
- derived consistency。

---

## Phase 6：Advanced Tools

完成：

- Raw Metadata JSON；
-完整 Binary Viewer；
- Header / Packet Inspector。

---

## Phase 7：Data 页面完善

先完成表格。

之后再考虑：

- waveform；
- channel statistics；
- large dataset downsampling。

这些不是当前重构第一优先级。

---

# 58. 第一版验收标准

重构后的第一版至少应满足：

### 文档安全

- 打开文件默认只读；
- 修改必须进入 Draft；
- 原文件不会被修改；
- Edit Draft 只能 Save As；
- Dirty state 正确。

### Metadata

- 普通用户无需查看 JSON 即可编辑主要 Metadata；
- Header / Version 自动且只读；
- Units 有默认值；
- BoundingBox 自动；
- ElevationNum 自动；
- ChannelNum 自动；
- ChannelNo 自动；
- Direction 自动；
- Sampling/DT 正确联动。

### Channel

- 可以新增；
- 可以复制；
- 可以删除；
- ChannelID 自定义；
- ChannelID 校验；
- 默认配置继承；
- table/detail 联动。

### Geometry

- 可显示建筑线框；
- 可显示各 Elevation；
- 可显示 Channel Location；
- 可显示 Azimuth；
- 表格与图形选择联动。

### Data

- 能正确显示数据表格；
- Channel 与数据对应关系不会因为 UI 排序改变。

### Validation

- 保存前完整校验；
- Error 阻止保存；
- Warning 可以明确展示。

### Advanced

- Raw JSON 可以查看和编辑；
- Binary Viewer 可以查看完整文件；
- Binary Viewer 只读。

---

# 59. 实施优先级原则

遇到旧实现与新版设计冲突时：

```text
正确的数据模型
>
清晰的用户逻辑
>
代码结构
>
兼容旧 UI
```

即：

> **优先保证新版本逻辑正确、界面清晰、架构合理，而不是维持旧代码表面上的连续性。**

如果直接重写一个组件比修改旧组件更简单、更稳定，就直接重写。

MATLAB 参考代码也遵循同样原则：

> 继承数学逻辑和工程思想，不要求逐行翻译 MATLAB 实现。