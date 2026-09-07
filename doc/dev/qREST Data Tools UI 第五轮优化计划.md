下面整理成一份适合作为下一轮小版本迭代依据的开发文档。本轮不再调整整体架构，主要解决三个已经明确的问题：**Help 文档加载、StartTime 时区、已有数据情况下的通道增删/追加**。

# qREST Data Tools 小版本优化开发文档

## 1. 本轮目标

当前 GUI 已具备较完整的数据编辑和查看能力。本轮重点不是继续扩展页面，而是在现有设计基础上完善三个实际使用中暴露的问题：

1. 修复程序内 User Guide / File Format 文档无法打开的问题；
2. 修复 StartTime 缺少时区导致 Validation 报错的问题；
3. 在保持 Channels 页面结构安全的前提下，支持已有数据文件继续追加和删除通道。

总体原则：

> 不解除现有 Metadata / Packet 一致性保护，而是通过“数据感知”的操作同步修改 Channel Metadata 与 Packet Body。

---

# 2. Help 文档加载修复

## 2.1 当前问题

当前 `DocumentViewerDialog.qml` 使用：

```qml
XMLHttpRequest
```

读取：

```text
qrc:/.../helper.md
qrc:/.../file_format.md
```

Qt 默认禁止 QML XHR 读取本地资源，因此出现：

```text
XMLHttpRequest: Using GET on a local file is disabled by default.
```

随后继续访问 request 状态，进一步产生：

```text
Error: Invalid state
```

当前文档实际上已经正确加入 qrc，因此问题主要在读取方式，而不是资源注册。

## 2.2 修改方案

不使用：

```text
QML_XHR_ALLOW_FILE_READ
```

作为正式方案。

新增统一的 C++ 文本资源读取接口，例如：

```cpp
Q_INVOKABLE QString readTextResource(const QString &url) const;
```

负责将：

```text
qrc:/qt/qml/...
```

转换为：

```text
:/qt/qml/...
```

并通过：

```cpp
QFile
```

读取 UTF-8 文本。

---

## 2.3 DocumentViewerDialog 简化

`DocumentViewerDialog.qml` 不再负责文件读取，只负责：

```text
标题
Markdown 内容
滚动显示
Close
```

接口建议类似：

```text
openDocument(title, text, source)
```

调用流程：

```text
Help Menu
    ↓
ViewModel.readTextResource()
    ↓
QString Markdown
    ↓
DocumentViewerDialog
```

读取失败时显示明确错误提示，不要把底层异常直接当作 Markdown 正文。

---

## 2.4 Field Help 顺带检查

本轮同时检查：

```text
Description.json
FieldHelpRegistry
```

的读取方式。

如果同样使用 `XMLHttpRequest` 访问 qrc，则一并改用统一的 C++ resource loader。

目标是：

```text
Markdown
JSON
其他内置文本资源
```

统一通过同一套资源读取逻辑访问。

---

# 3. StartTime 时区问题

## 3.1 当前问题

Core 对 `DataInfo.StartTime` 要求严格 ISO 8601：

```text
2026-09-06T22:30:00.000+08:00
```

或：

```text
2026-09-06T14:30:00.000Z
```

不接受：

```text
2026-09-06T22:30:00.000
```

因为后者没有时区信息。

当前 GUI 在 `updateStartTimestamp()` 和 Packet Header 更新时间时使用：

```cpp
QDateTime::fromMSecsSinceEpoch(timestamp)
    .toString(Qt::ISODateWithMs)
```

生成的字符串可能没有显式 `Z` 或 UTC offset，因此随后 Validation 报：

```text
StartTime must include Z or timezone offset
```

---

## 3.2 修改原则

保留 Core 当前严格要求。

不要通过放宽 Validation 来解决问题。

应修复 GUI 的时间格式化逻辑。

---

## 3.3 推荐格式

GUI 时间选择器继续按照：

> 当前系统本地时间

解释用户输入。

Metadata 保存时使用：

```text
YYYY-MM-DDTHH:mm:ss.zzz±HH:mm
```

例如：

```text
2026-09-06T22:30:00.000+08:00
```

Packet Header 中仍保存对应的 Unix milliseconds。

二者必须描述同一时刻。

---

## 3.4 建立统一 formatter

不要让多个函数分别格式化时间。

增加统一 helper，例如：

```cpp
QString formatTimestampWithLocalOffset(qint64 timestampMs);
```

统一供：

```text
updateStartTimestamp()
updatePacketHeader()
其他 StartTime 写入位置
```

使用。

---

## 3.5 TimePicker 小型优化

当前 TimePicker 继续保留：

```text
年 / 月 / 日 / 时 / 分 / 秒
```

即可。

建议增加只读提示：

```text
Time Zone: Local System Time (UTC+08:00)
```

具体 offset 根据当前系统时区动态显示。

暂不增加复杂的时区选择器。

---

# 4. 通道编辑总体原则

目前：

```cpp
canEditChannelOrder()
```

在 Packet Body 已存在时禁止：

```text
Add
Duplicate
Delete
```

这是合理的保护设计，因为 Metadata Channels 与 Packet channel-major 数据必须严格一一对应。

本轮：

> **继续保留 Channels 页面这一锁定规则。**

不要简单改成“有数据也能直接 Add/Delete Metadata”。

新的通道数量变化必须通过同时修改：

```text
Metadata
+
Packet Body
```

完成。

---

# 5. 新增 Add Channel Data 功能

## 5.1 功能定位

Data 页面新增：

```text
Add Channels...
```

现有：

```text
Import Data
```

仍然表示：

> Replace / 导入完整数据集。

新的：

```text
Add Channels
```

表示：

> 向当前已有数据集追加 1～N 个新的通道。

两者语义必须明确区分。

---

# 6. Append Channels 数据规则

当前 qREST 数据采用 channel-major 排布：

```text
CH1 complete data
CH2 complete data
CH3 complete data
...
```

DataTableModel 也是按：

```cpp
channel * NPTS + row
```

读取。

因此追加通道可以直接：

```text
existing channel blocks
+
incoming channel blocks
```

无需重排所有采样点。

---

# 7. Append Channels Compatibility

第一版采用严格规则。

## 必须一致

### NPTS

```text
incoming.sample_count == existing.NPTS
```

否则拒绝追加。

---

### Sampling Rate

如果输入源包含采样率：

```text
incoming.fs == existing.fs
```

否则拒绝。

本轮不自动重采样。

---

### StartTime

如果输入格式能够提供起始时间，则应检查：

```text
incoming.StartTime == existing.StartTime
```

或在允许的极小时间误差范围内一致。

TXT/CSV 当前没有时间信息，可以：

> 默认由用户确认其与当前数据时间范围一致。

---

# 8. ExternalDataset 时间信息扩展

当前 `ExternalDataset` 只有：

```text
source_format
channel_count
sample_count
sample_rate_hz
channel_labels
channel_sequential_data
```

如果 TDMS / MiniSEED / HDF5 能可靠取得起始时间，建议增加可选字段：

```cpp
std::optional<std::uint64_t> start_time_ms;
```

这样后续 Compatibility 可以统一检查：

```text
NPTS
Sampling Rate
StartTime
```

这是对多文件逐次补充测点数据比较重要的增强。

---

# 9. Add Channels UI

Data 页面建议调整为：

```text
[ Import / Replace Data ]
[ Add Channels... ]
[ Export Data ]
```

点击 Add Channels 后显示 Preview，例如：

```text
Current Dataset
Channels:      6
Samples:       180000
Sampling Rate: 100 Hz

Incoming Data
Channels:      3
Samples:       180000
Sampling Rate: 100 Hz

Result
Channels:      9

Compatibility: OK
```

确认后再正式修改 Draft。

---

# 10. 新增 Channel Metadata

追加 N 个数据通道以后，同时自动创建 N 个 Metadata Channel。

现有 `makeDefaultChannel()` 已提供较好的默认值：

```text
ChannelNo    自动
ChannelID    UNKNOWN
DeviceType   Accelerometer
Measurand    Acceleration
Scale        1.0
Azimuth      -1
XYZ          0,0,0
```

并可以继承已有通道的通用 DeviceType / Measurand / Scale。

本轮继续复用。

---

# 11. 新增后的用户提示

由于：

```text
Azimuth = -1
XYZ = 0,0,0
```

只是占位值，所以追加完成后明确提示：

```text
已添加 3 个数据通道并创建默认 Channel 信息。
请前往 Channels 页面检查 ChannelID、Azimuth 和 LocationXYZ。
```

如果方便，可以提供：

```text
Review New Channels
```

直接切换到 Channels 页面并选中第一个新增通道。

---

# 12. 外部格式同样支持 Append

当前 External Import 已支持：

```text
TDMS
MiniSEED
HDF5
Async loading
Channel Mapping
```

但最终行为目前主要是替换整个 Packet。

建议统一引入概念：

```cpp
enum class DataImportMode {
    Replace,
    AppendChannels
};
```

从而：

```text
TXT / CSV
TDMS
MiniSEED
HDF5
```

最终都可以走：

```text
ExternalDataset
       ↓
Replace
   or
AppendChannels
```

不要为每种格式分别实现一套通道追加逻辑。

---

# 13. Core / Document 层增加原子 Append 操作

Append 的核心逻辑不要写进 QML。

建议在 Core 或 Document/Application 层增加类似：

```text
appendChannelDataset(...)
```

负责完整处理：

```text
Compatibility
    ↓
Append packet channel blocks
    ↓
Create default Channels
    ↓
Renumber ChannelNo
    ↓
Update ChannelNum
    ↓
Rebuild DataPacket
    ↓
Update Metadata
    ↓
Mark Draft dirty
```

关键要求：

> Metadata 与 Packet 必须原子更新。

任何一步失败，都保持原文档不变。

---

# 14. 删除已有 Channel

已有 Packet 数据时也可以逐步支持 Delete。

删除 Channel `k` 时：

```text
删除 metadata.Channels[k]
+
删除 packet 中对应的 NPTS 数据块
+
重新编号 ChannelNo
+
更新 channel_count
```

因为 Packet 是 channel-major，因此这一操作结构上比较简单。

---

# 15. 删除 UI

有数据时 Channels 页的 Delete 可以改成数据感知操作。

点击：

```text
Delete
```

弹出确认：

```text
Delete Channel 5?

This will also remove all data belonging to this channel
(180000 samples).

Cancel
Delete Channel + Data
```

只有明确确认后才执行。

---

# 16. Add / Duplicate 在已有数据时的策略

继续保持：

### 无数据

Channels 页面：

```text
Add
Duplicate
Delete
```

正常工作。

### 已有数据

```text
Add
Duplicate
```

仍禁止直接修改结构。

增加数据通道使用：

```text
Data → Add Channels
```

Delete 可以升级成：

```text
Delete Channel + Data
```

这样避免不同页面各自随意改变 Channel 数量。

---

# 17. 推荐实现顺序

## Phase 1：确定性 Bug

先处理：

1. Help 文档改为 C++ QFile/qrc 读取；
2. 检查 Field Help JSON 的资源读取方式；
3. 修复 StartTime formatter；
4. TimePicker 增加本地时区提示。

这些改动小、风险低。

---

## Phase 2：Append Channels Core

实现：

```text
Append compatibility
Append packet data
Create default channels
Atomic Metadata + Packet update
```

先不做复杂 UI。

---

## Phase 3：TXT / CSV Add Channels

在 Data 页面增加：

```text
Add Channels...
```

先支持最简单的 TXT/CSV：

```text
1 ~ N columns
```

并严格检查：

```text
NPTS
```

---

## Phase 4：External Import Append

在已有 ExternalImportDialog 基础上加入：

```text
Replace
Append Channels
```

让：

```text
TDMS
MiniSEED
HDF5
```

共享同一 Append 机制。

---

## Phase 5：Delete Channel + Data

完成安全删除：

```text
Channel Metadata
+
Packet channel block
```

并加入确认 Dialog。

---

# 18. 本轮优先级

| 优先级 | 内容                                  |
| --- | ----------------------------------- |
| P0  | Help 文档读取改为 C++ resource loader     |
| P0  | StartTime 输出带明确时区                   |
| P1  | Append Channels 底层原子操作              |
| P1  | Data 页面增加 Add Channels              |
| P1  | 自动创建默认 Channel Metadata             |
| P1  | NPTS / fs / StartTime Compatibility |
| P2  | External Import 支持 AppendChannels   |
| P2  | Delete Channel + Data               |
| P2  | ExternalDataset 增加可选 StartTime      |
| P2  | 新增后自动跳转/选中新 Channel                 |

---

# 19. 本轮验收标准

完成后应满足：

* Help → User Guide 可以正常打开；
* Help → File Format Specification 可以正常打开；
* 不依赖 `QML_XHR_ALLOW_FILE_READ`；
* Field Help 资源读取机制稳定；
* GUI 创建的 StartTime 始终包含 `Z` 或明确 UTC offset；
* Metadata StartTime 与 Packet timestamp 表示同一时刻；
* 已有数据时仍不能在 Channels 页面单独增加 Metadata Channel；
* Data 页面能够向现有数据追加 1～N 个通道；
* 追加后 Packet / Metadata / ChannelNum 自动保持一致；
* 新增 Channel 自动生成默认 Metadata；
* 不兼容的 NPTS / fs / StartTime 不允许直接追加；
* 可以安全删除 Channel 及其对应数据；
* 所有结构性修改仍然发生在 Draft 中，不直接修改源文件。

这一轮完成后，qREST Data Tools 的编辑模型会从目前的“**通过限制避免不一致**”，进一步发展成“**允许结构变化，同时由程序主动维护一致性**”，这是当前工具从可用走向实用比较关键的一步。
