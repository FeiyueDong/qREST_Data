# qREST Data Tools 用户指南

qREST Data Tools 是用于创建、查看、编辑、导入和检查 qREST 数据文件的桌面工具。

程序主要用于完成以下工作：

- 创建新的 qREST 数据文件；
- 查看已有 qREST 文件；
- 编辑建筑、测点和数据相关信息；
- 导入 TXT / CSV、TDMS、Modified MiniSEED 和 HDF5 数据；
- 向已有数据追加新的通道；
- 删除通道及其对应数据；
- 检查测点的空间布置；
- 验证 Metadata 与 Packet 数据的一致性；
- 查看 Metadata JSON、Packet Header 和原始二进制内容。

本指南主要介绍软件的实际使用方法。

如需了解 qREST 文件的严格字段定义、二进制结构和数据格式，请查看：

**Help → qREST File Format Specification**

---

# 1. 快速开始

创建一个新的 qREST 文件通常可以按照以下流程完成：

1. 点击 **New** 创建新的 qREST 文件；
2. 在 **Building** 页面填写工程、建筑和楼层信息；
3. 在 **Channels** 页面添加并配置测点通道；
4. 在 **Data** 页面填写数据基本信息并导入监测数据；
5. 点击工具栏中的 **Validate** 检查当前文件；
6. 处理 Validation 中的 Error；
7. 点击 **Save As** 保存 qREST 文件。

基本工作流程可以概括为：

```text
New
 ↓
Building
 ↓
Channels
 ↓
Data
 ↓
Validate
 ↓
Save As
```

建议在保存正式文件前至少执行一次完整 Validation。

如果需要修改已有 qREST 文件，推荐：

```text
Open
 ↓
Edit
 ↓
Modify
 ↓
Validate
 ↓
Save As
```

---

# 2. 文件与编辑模式

qREST Data Tools 对已有文件采用保护性的编辑方式，避免误操作直接修改原始数据。

## 2.1 New

点击：

**File → New**

或工具栏中的 **New**，创建一个新的 qREST 文件。

新文件进入：

**New Draft**

状态，可以直接修改。

---

## 2.2 Open

点击：

**File → Open**

选择已有 qREST 文件。

打开后的文件默认处于：

**Read Only**

状态。

此时可以查看所有信息，但不能直接修改原始文件。

---

## 2.3 Edit

需要修改已有文件时，点击工具栏中的：

**Edit**

程序会创建当前文件的可编辑副本，并进入：

**Editing Copy**

状态。

之后的修改只作用于当前 Draft，不直接修改原始文件。

---

## 2.4 Save As

完成修改后，通过：

**Save As**

将当前 Draft 保存为新的 qREST 文件。

编辑已有文件时，程序会保护原始源文件，不能直接通过 Save As 覆盖原文件。

成功保存后，新文件成为当前只读文档。

---

## 2.5 未保存修改

如果当前 Draft 中存在尚未保存的修改，执行 New、Open 或关闭程序等操作时，程序会提示是否放弃当前修改。

在重要编辑完成后，建议先执行 Validation，再使用 Save As 保存。

---

# 3. 主界面

主界面主要由工具栏和以下页面组成：

- **Overview**
- **Building**
- **Channels**
- **Data**
- **Validation**

工具栏提供常用操作：

- **New**：创建新文件；
- **Open**：打开已有文件；
- **Edit**：创建可编辑副本；
- **Validate**：执行完整检查；
- **Save As**：保存当前 Draft；
- **JSON**：打开 Raw Metadata JSON；
- **Binary**：打开 Binary Viewer。

窗口顶部和状态区域会显示当前文件及编辑状态。

---

# 4. Overview

**Overview** 页面用于快速查看当前 qREST 文件的整体状态。

页面主要汇总：

- 当前文件和编辑模式；
- 工程和建筑基本信息；
- 通道数量；
- 数据采样信息；
- 数据时长；
- Validation 状态；
- qREST 文件格式信息。

Overview 主要用于快速浏览，不承担主要编辑功能。

需要修改具体内容时，请进入对应的 **Building**、**Channels** 或 **Data** 页面。

---

# 5. Building

**Building** 页面用于设置建筑和工程相关 Metadata。

主要包括：

- Document / Units；
- Building Information；
- Geo Location；
- Structural Footprint；
- Elevation。

---

## 5.1 Document 与 Units

页面会显示 qREST Metadata 的 Header 和 Version。

这些属于格式固定或程序维护的信息，通常不需要用户修改。

长度相关数据可以使用不同 Distance Unit，例如：

- m
- cm
- mm

时间单位当前固定为：

**s**

建筑尺寸、楼层标高和测点位置应采用统一的长度单位。

---

## 5.2 Project Information

常用字段包括：

- **Project Name**：工程或监测项目名称；
- **Structural Type**：建筑结构类型。

这些字段用于描述监测对象本身。

---

## 5.3 Geo Location

Geo Location 用于描述建筑地理位置以及局部坐标方向。

主要包括：

- **Longitude**
- **Latitude**
- **North Angle**

North Angle 描述建筑局部坐标系与地理北向之间的关系。

如不确定字段含义，可将鼠标停留在字段标签旁的帮助标记上查看 Field Help。

---

## 5.4 Structural Footprint

Structural Footprint 用于描述建筑平面轮廓。

目前支持：

### Rectangular

矩形建筑轮廓。

主要输入：

- Length
- Width

### Circular

圆形建筑轮廓。

主要输入：

- Radius

### Polygon

任意多边形轮廓。

需要输入一组平面顶点坐标，每个顶点使用 X、Y 两个坐标表示。

程序会根据当前轮廓自动计算 **Bounding Box**，通常不需要手动维护。

---

## 5.5 Elevation

Elevation 用于定义建筑各楼层或监测高度。

例如：

```text
0
3.6
7.2
10.8
14.4
```

也可以使用逗号、空格等方式分隔。

Elevation 必须按 **从低到高严格递增** 的顺序填写，不能包含重复值。

程序会根据 Elevation 自动计算 ElevationNum。

---

# 6. Channels

**Channels** 页面用于配置每一个监测通道的信息。

左侧为 Channel List，右侧显示当前选中通道的详细信息和 Sensor Layout。

---

## 6.1 Channel List

每一行代表一个 qREST 数据通道。

常见字段包括：

- ChannelNo
- ChannelID
- DeviceType
- Measurand
- Direction
- Scale
- Azimuth
- X / Y / Z

点击列表中的某个通道，可以在右侧查看和编辑该通道。

---

## 6.2 无数据时的通道管理

当当前文档尚未包含 Packet Body 数据时，可以直接使用：

- **Add**
- **Duplicate**
- **Delete**

管理 Channel Metadata。

### Add

创建一个新的默认通道。

### Duplicate

复制当前通道的大部分配置，便于建立相似测点。

复制后的 ChannelID 需要重新检查。

### Delete

删除当前选中的 Metadata Channel。

---

## 6.3 已有数据时的通道管理

当文件已经包含实际 Packet Body 数据后，**Add** 和 **Duplicate** 会被锁定。

这是为了保证：

**Metadata Channels 与 Packet 中的实际数据通道始终保持一致。**

已有数据时，如果需要增加通道，应使用：

**Data → Add Channels...**

如果需要删除通道，仍可在 Channels 页面点击 **Delete**。程序会提示：

**Delete Channel + Data**

确认后会同时删除：

- 当前 Channel Metadata；
- Packet 中属于该通道的全部采样数据。

删除完成后，ChannelNo、ChannelNum 和 Packet channel count 会同步更新。

---

## 6.4 ChannelID

ChannelID 是通道标识。

可以根据实际监测系统自行定义，例如：

```text
ACC-RF-01
ACC-05F-X
SENSOR-A12
```

普通 ChannelID 应保持唯一。

如果当前无法确定 ChannelID，可以使用：

```text
UNKNOWN
```

多个 UNKNOWN 是允许的。

---

## 6.5 DeviceType

DeviceType 描述该通道对应的设备或传感器类型，例如：

- Accelerometer
- Velocity Sensor
- Displacement Sensor
- Strain Gauge
- Temperature Sensor
- Unknown
- Other

---

## 6.6 Measurand

Measurand 描述该通道测量的物理量，例如：

- Acceleration
- Velocity
- Displacement
- Strain
- Temperature
- Other

---

## 6.7 Scale

Scale 是该通道数据对应的缩放系数。

其取值应与原始数据来源和采集系统保持一致。

如果数据已经在导入前完成物理量转换，应确认 Scale 与当前数据定义一致。

Scale 不能为 0。

---

## 6.8 Azimuth 和 Direction

Azimuth 用于描述传感器测量方向。

水平面基本方向约定为：

```text
            +Y
             0°
              ↑
              |
270° / -X ← --+-- → +X / 90°
              |
              ↓
            180°
```

特殊值：

```text
Azimuth = -1
```

表示竖向测量通道。

**Direction** 由程序根据 Azimuth 自动判断，仅用于显示，不需要单独填写。

---

## 6.9 LocationXYZ

LocationXYZ 表示测点在建筑局部坐标系中的空间位置。

包括：

- X
- Y
- Z

其中 X、Y 表示平面位置，Z 表示高度。

坐标单位与 Building 页面中的 Distance Unit 一致。

---

# 7. Sensor Layout

Channels 页面右侧提供 Sensor Layout，用于辅助检查测点空间位置和测量方向。

Sensor Layout 根据：

- Structural Footprint；
- Elevation；
- LocationXYZ；
- Azimuth；

生成轻量建筑和测点示意图。

它主要用于 Metadata 检查，并不是有限元模型。

---

## 7.1 Isometric

**Isometric** 用于查看整体空间布置。

适合快速检查：

- 测点是否位于合理楼层；
- 测点是否明显超出建筑；
- 各测点及测量方向的大致空间关系。

楼层使用统一半透明平面进行辅助显示。

---

## 7.2 Plan

**Plan** 显示 X-Y 平面。

适合准确检查：

- X 坐标；
- Y 坐标；
- 平面测点位置；
- 水平测量方向。

---

## 7.3 X-Z

**X-Z** 视图用于检查：

- X 位置；
- Z 高度。

---

## 7.4 Y-Z

**Y-Z** 视图用于检查：

- Y 位置；
- Z 高度。

---

## 7.5 Floor Filter 与 Sensor Hover

对于楼层较多的建筑，可以使用楼层筛选减少图形重叠。

鼠标停留在 Sensor 附近时，可以查看相关通道和位置等信息。

Sensor Layout 与 Channel List 联动，可辅助定位和检查通道。

---

# 8. Data

**Data** 页面用于配置当前记录的数据基本信息和 Packet Body。

主要内容包括：

- Event Name；
- StartTime；
- Sampling Rate；
- Sampling Interval；
- NPTS；
- Corrected；
- 数据矩阵。

页面同时提供：

- **Import / Replace Data**
- **Add Channels...**
- **Export Data**
- **Packet Header...**

---

## 8.1 Event Name

Event Name 用于描述当前数据记录，例如：

```text
Ambient_2026_09_01
Myanmar_M7.7
Daily_Record_001
```

命名方式可根据工程需要自行确定。

---

## 8.2 StartTime

StartTime 表示当前记录第一个采样点对应的起始时间。

建议使用 **Set** 按钮设置时间。

时间选择器按照当前计算机的 **Local System Time** 输入，并显示当前 UTC Offset，例如：

```text
Time Zone: Local System Time (UTC+08:00)
```

Metadata 中会保存包含明确 UTC Offset 的 ISO 8601 时间，例如：

```text
2026-09-14T13:30:00.000+08:00
```

这样可以避免不同计算机和时区之间产生时间歧义。

---

## 8.3 Sampling Rate

Sampling Rate 表示每秒采样次数，单位为 Hz，例如：

```text
50 Hz
100 Hz
200 Hz
```

Sampling Interval 由程序根据 Sampling Rate 自动计算。

---

## 8.4 NPTS

NPTS 表示每一个通道包含的采样点数量。

导入实际数据后，NPTS 应与 Packet Body 的实际采样点数一致。

当 Packet 已包含数据时，NPTS 不应单独修改为与数据不一致的值。

---

## 8.5 Corrected

Corrected 用于记录当前数据是否已经经过相应的数据修正或校正处理。

请根据实际数据处理状态选择合适的值。

---

# 9. TXT / CSV：替换完整数据

点击：

**Import / Replace Data**

或：

**Data → Import Data Body → Text / CSV Replace...**

可以使用 TXT / CSV 矩阵替换当前 Packet Body。

输入文件应满足：

```text
每一行 = 一个采样时刻
每一列 = 一个通道
```

例如：

```text
0.120   0.031  -0.012
0.138   0.040  -0.018
0.111   0.026  -0.010
```

表示：

```text
3 个采样点
3 个通道
```

文件可以使用空格、Tab 或逗号分隔数值。

导入时程序会检查：

- 文件是否为空；
- 每一行列数是否一致；
- 数值是否可以解析；
- 当前 NPTS 与导入行数是否一致；
- 当前 Channels 数量与导入列数是否一致。

如果当前 Metadata 中没有 Channel，程序可以根据导入列数建立默认 Channel。

如果当前已经配置了 Channels，则导入矩阵列数必须与当前 Channel 数一致。

当导入行数与已有 NPTS 不一致时，程序会提示用户确认是否采用导入数据的 NPTS。

---

# 10. TXT / CSV：追加新的数据通道

当当前文件已经包含 Packet Body 数据时，可以点击：

**Data → Add Channels...**

或：

**Data → Import Data Body → Text / CSV Add Channels...**

选择一个新的 TXT / CSV 文件，将其中的一列或多列作为新的 qREST 通道追加到当前数据之后。

---

## 10.1 Add Channels Preview

选择文件后，程序会先显示 Preview。

Preview 包含：

```text
Current Dataset
Channels
Samples
Sampling Rate

Incoming Data
Channels
Samples
Sampling Rate
StartTime

Result
Channels

Compatibility
```

只有当显示：

```text
Compatibility: OK
```

时，才可以执行 Add Channels。

---

## 10.2 兼容性要求

追加数据时，程序会保护已有数据结构。

主要检查包括：

- 当前文件必须已经存在有效 Packet 数据；
- 当前 Metadata Channel 数必须与 Packet Channel 数一致；
- 新数据必须包含有效通道和采样点；
- 新数据的 NPTS 必须与已有数据相同；
- 如果新数据包含 Sampling Rate，则必须与当前数据一致；
- 如果新数据包含 StartTime，则必须与当前 Packet StartTime 一致；
- 追加后通道数量不能超过 Packet Header 可表示范围。

对于普通 TXT / CSV 文件，文件本身通常不携带 Sampling Rate 和 StartTime，因此主要根据 NPTS 和当前文档状态进行检查。用户应自行确认该文件与当前数据记录属于同一时间段。

程序不会在 Add Channels 操作中自动截断、补齐或重采样不兼容的数据。

---

## 10.3 新增通道的默认 Metadata

成功追加数据后，程序会自动创建相同数量的 Channel Metadata。

新增 Channel 默认使用：

```text
ChannelID   = UNKNOWN
Azimuth     = -1
LocationXYZ = 0, 0, 0
```

DeviceType、Measurand 和 Scale 会尽量继承当前已有通道的通用设置。

追加完成后，程序会选中第一个新增 Channel。

请进入 **Channels** 页面重点检查并补充：

- ChannelID；
- DeviceType；
- Measurand；
- Scale；
- Azimuth；
- LocationXYZ。

---

# 11. External Data Import

qREST Data Tools 支持从其他监测数据格式导入数据。

目前主要支持：

- TDMS；
- Modified MiniSEED；
- HDF5。

可以通过：

**Data → Import External Data**

打开对应导入功能。

TDMS 和 Modified MiniSEED 支持文件或目录形式导入；HDF5 以文件形式导入。

---

## 11.1 基本流程

外部数据导入通常包含：

```text
Select File / Directory
        ↓
Import Options
        ↓
Preview
        ↓
Choose Import Mode
        ↓
Mapping / Compatibility
        ↓
Apply
```

外部数据在后台读取，读取过程中会显示 Loading 状态，避免主界面长时间冻结。

---

## 11.2 Import Mode

External Import 支持两种模式：

### Replace Dataset

使用外部数据替换当前完整数据集。

此模式下需要进行 **Channel Mapping**，将外部数据通道对应到当前 qREST Channel。

### Append Channels

把外部数据作为新的 qREST Channel 追加到当前数据集。

此模式不会覆盖已有数据，而是在已有通道后增加新的数据通道，并自动创建默认 Channel Metadata。

Append Channels 使用与 TXT / CSV Add Channels 相同的兼容性原则。

---

## 11.3 Preview

读取完成后，程序会显示外部数据基本信息，例如：

- Format；
- Path；
- Channel Count；
- Sample Count；
- Sample Rate；
- Channel Label。

建议在 Apply 前检查 Preview 是否符合预期。

---

## 11.4 Channel Mapping

在 **Replace Dataset** 模式下，需要确认：

```text
External Channel
        ↓
qREST Channel
```

每个外部 Channel 都必须映射到一个有效的 qREST Channel，并避免重复映射。

请重点检查：

- 通道顺序；
- 水平与竖向方向；
- 对应测点是否正确。

不要仅根据外部文件中的编号假定其一定与 qREST ChannelNo 相同。

在 **Append Channels** 模式下，新数据直接作为新的 Channel 加入，因此不使用已有 Channel Mapping。

---

## 11.5 TDMS Options

TDMS Import 提供常用参数，包括：

- Target Unit；
- Sensitivity；
- Time Verification；
- Raw Counts。

Advanced 中还可以设置：

- Explicit Sensitivity；
- Storage Scale；
- Post Scale。

如果不清楚这些参数的含义，建议优先使用默认配置，并确认导入后的数据单位和数值是否符合原始采集系统定义。

---

## 11.6 Modified MiniSEED Options

Modified MiniSEED Import 提供：

- Group；
- Gap Policy；
- Include Dimensionless。

Gap Policy 可以选择：

- Fill NaN；
- Error；
- Ignore。

应根据原始 MiniSEED 文件中的时间连续性和缺测情况选择合适策略。

---

## 11.7 HDF5

HDF5 文件可以作为外部数据导入。

同时，当前 qREST 数据也可以通过：

**Data → Export → HDF5...**

导出为 HDF5。

---

# 12. 删除已有数据通道

当 Packet Body 已经存在时，Channels 页仍允许删除选中的 Channel。

点击 **Delete** 后，程序会提示：

```text
Delete Channel + Data
```

确认后将同时删除：

- 该 Channel Metadata；
- 该 Channel 对应的全部 NPTS 数据。

其他 Channel 的数据顺序保持不变，随后程序会自动：

- 重新编号 ChannelNo；
- 更新 ChannelNum；
- 更新 Packet channel count。

如果删除的是最后一个 Channel，Packet NPTS 会同时归零。

删除属于结构性操作，建议执行后重新运行 Validation。

---

# 13. Data Matrix 与数据复制

Data 页面下方表格用于查看当前 Packet Body。

表格中：

```text
每一列 = 一个 Channel
每一行 = 一个采样点
```

左侧时间标记根据 Sampling Rate 计算。

可以使用：

```text
Ctrl + A
```

选择全部数据。

使用：

```text
Ctrl + C
```

复制当前选中的数据。

也可以点击行表头或列表头进行整行、整列选择。

---

# 14. 数据导出

## 14.1 Text

通过：

**Data → Export → Text...**

可以将当前 Packet Body 导出为文本矩阵。

导出格式仍采用：

```text
每一行 = 一个采样点
每一列 = 一个 Channel
```

列之间使用 Tab 分隔。

---

## 14.2 HDF5

通过：

**Data → Export → HDF5...**

可以将当前 Metadata 和数据导出为 HDF5。

---

## 14.3 Metadata JSON

通过：

**Data → Export Metadata JSON...**

可以单独导出当前 Metadata JSON。

对应地：

**Import Metadata JSON...**

可将 JSON Metadata 导入当前可编辑 Draft。

---

# 15. Validation

Validation 用于检查当前 qREST 文件是否存在格式或数据一致性问题。

点击工具栏中的：

**Validate**

后，程序会执行完整检查并切换到 Validation 页面。

检查结果主要分为：

- Error
- Warning
- Info

---

## 15.1 Error

Error 表示当前文件存在需要修正的问题。

例如：

- Metadata Channel 数与 Packet 不一致；
- NPTS 不一致；
- Sampling Rate 或 DT 不合法；
- StartTime 不合法；
- 必要字段缺失；
- Packet 数据长度与数据维度不一致。

Save As 前会执行 Final Validation；如果仍存在 Error，程序会阻止保存。

---

## 15.2 Warning

Warning 表示某些内容需要用户确认，但不一定意味着文件非法。

例如：

- Sensor 位于 Bounding Box 外；
- Sensor 的 Z 超出 Elevation 范围；
- BoundingBox 与当前 Footprint 参数不一致。

Warning 是否需要修改，应结合实际工程情况判断。

---

## 15.3 Info

Info 用于显示普通检查结果或状态信息。

例如：

```text
Validation passed
```

表示当前检查没有发现 Error 或 Warning。

---

# 16. Advanced Tools

Advanced 菜单提供面向高级用户和调试场景的工具。

普通用户通常不需要频繁使用。

---

## 16.1 Raw Metadata JSON

打开：

**Advanced → Raw Metadata JSON**

可以查看当前完整 Metadata JSON。

在可编辑 Draft 中也可以修改并 Apply JSON。

程序会恢复固定格式字段并重新计算部分派生 Metadata。

普通情况下建议优先使用：

- Building；
- Channels；
- Data；

页面进行结构化编辑。

---

## 16.2 Binary Viewer

Binary Viewer 用于查看完整 qREST 文件的二进制内容。

主要显示：

- Offset；
- Hex Bytes；
- ASCII。

同时支持按 Offset 跳转，以及 ASCII / Hex 搜索。

该功能主要用于数据格式检查和调试。

---

## 16.3 Header / Packet Inspector

Header / Packet Inspector 用于查看底层：

- File Header；
- Packet Header。

在可编辑 Draft 中可以修改部分 Packet Header 信息。

Packet Header 修改会同步更新相应 Metadata，例如：

- ChannelNum；
- NPTS；
- DT；
- Frequency；
- StartTime。

如果当前已经包含 Packet Body，程序会限制通过 Packet Header 随意改变 Channel 数量，避免数据矩阵被错误重排。

如果不熟悉 qREST 文件格式，不建议直接修改底层 Packet 信息。

---

# 17. Field Help

Building、Channels、Data 和部分高级界面中的主要字段支持悬浮帮助。

将鼠标停留在字段标签或帮助标记附近，可以查看简短中文说明。

Field Help 主要回答：

> “这个字段是什么意思？”

如果需要完整、严格的字段定义，请查看：

**Help → qREST File Format Specification**

---

# 18. Help 菜单

Help 菜单提供：

- **User Guide**：打开本用户指南；
- **qREST File Format Specification**：查看 qREST 文件存储格式规范；
- **Project Homepage**：打开项目主页；
- **About qREST Data Tools**：查看程序版本和支持的 qREST Format Version。

User Guide 和 File Format Specification 随程序资源一起提供，可以直接在程序内查看。

---

# 19. 常见问题

## 19.1 为什么打开文件后不能修改？

已有 qREST 文件默认以 **Read Only** 模式打开。

点击工具栏中的 **Edit** 创建 Editing Copy 后即可修改。

---

## 19.2 为什么不能直接覆盖原始文件？

这是程序的数据保护设计。

已有文件通过：

```text
Open
→ Edit
→ Save As
```

完成修改，可以降低误操作破坏原始监测数据的风险。

---

## 19.3 ChannelID 不知道时怎么办？

可以使用：

```text
UNKNOWN
```

作为未知 ChannelID。

UNKNOWN 可以在多个 Channel 中重复使用。

---

## 19.4 为什么已有数据后 Add 和 Duplicate 被锁定？

因为 Packet Body 中的数据通道必须与 Metadata Channels 严格对应。

如果只修改 Channel Metadata 而不修改 Packet，会产生不一致文件。

需要增加实际数据通道时，请使用：

**Data → Add Channels...**

或 External Import 中的：

**Append Channels**

模式。

---

## 19.5 已有数据时可以删除 Channel 吗？

可以。

在 Channels 页面选择 Channel 后点击 **Delete**，确认 **Delete Channel + Data**。

程序会同时删除 Channel Metadata 和该 Channel 的全部数据。

---

## 19.6 Add Channels 为什么显示 Compatibility Error？

重点检查：

- Incoming NPTS 是否与当前 NPTS 相同；
- Sampling Rate 是否一致；
- StartTime 是否一致（外部格式提供时间时）；
- 当前 Metadata Channel 数是否与 Packet Channel 数一致；
- 输入文件是否具有有效数据。

程序不会自动通过截断、补齐或重采样来强制兼容。

---

## 19.7 TXT / CSV Add Channels 为什么没有 Sampling Rate 和 StartTime？

普通文本矩阵通常只包含数据值，本身没有采样率和绝对时间信息。

因此 TXT / CSV Add Channels 主要检查 NPTS。

用户应确认追加文件与当前记录来自同一采样率和同一时间段。

---

## 19.8 External Import 中 Replace 和 Append 有什么区别？

**Replace Dataset**：

- 用外部数据替换完整数据集；
- 需要将外部 Channel 映射到当前 qREST Channel。

**Append Channels**：

- 保留当前已有数据；
- 把外部 Channel 追加为新的 qREST Channel；
- 自动创建默认 Channel Metadata。

---

## 19.9 为什么导入数据失败？

优先检查：

- 文件格式；
- Channel 数量；
- NPTS；
- Sampling Rate；
- Channel Mapping；
- 外部格式的导入参数。

---

## 19.10 为什么 Sensor 显示在建筑外？

检查：

- LocationXYZ；
- Distance Unit；
- Structural Footprint；
- Elevation。

也可以切换到 Plan / X-Z / Y-Z 视图进一步检查坐标。

---

## 19.11 为什么 Validation 有 Warning？

Warning 通常表示需要人工确认的问题，不一定意味着文件非法。

应根据实际工程情况判断是否需要修改。

---

## 19.12 StartTime 应该怎样设置？

建议使用 Data 页面中的 **Set**。

时间选择器使用当前系统本地时间，并保存明确的 UTC Offset。

例如：

```text
2026-09-14T13:30:00.000+08:00
```

不建议在 Raw Metadata JSON 中手工填写没有时区信息的 StartTime。

---

## 19.13 Raw Metadata JSON 可以直接修改吗？

在 Draft 状态下可以。

但 Raw JSON 属于高级编辑方式，普通修改建议优先使用结构化页面。

---

# 20. 使用建议

对于一般工程数据，推荐遵循以下原则：

1. 优先使用 Building、Channels、Data 页面进行结构化编辑；
2. 导入数据后运行一次 Validation；
3. Add Channels 前检查 Preview 和 Compatibility；
4. 删除带数据的 Channel 前确认选中的 Channel 是否正确；
5. External Import 时重点检查单位、Sampling Rate 和 Channel Mapping；
6. 通过 Sensor Layout 检查 LocationXYZ 和 Azimuth；
7. 正式保存前处理所有 Validation Error；
8. 对已有文件始终采用 Edit → Save As 工作流；
9. Raw Metadata JSON 和 Packet Inspector 主要用于高级检查和调试。

---

# 21. 获取更多信息

如果需要了解 qREST 文件格式本身，请打开：

**Help → qREST File Format Specification**

如果需要项目信息，可以打开：

**Help → Project Homepage**

如果需要查看当前程序版本及支持的 qREST Format Version，可以打开：

**Help → About qREST Data Tools**
