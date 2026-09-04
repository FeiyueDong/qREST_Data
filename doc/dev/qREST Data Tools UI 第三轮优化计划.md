# qREST Data Tools UI 本轮优化计划

## 1. 本轮目标

当前版本已经完成主要功能和基础架构，本轮不再进行大规模重构，重点优化：

- Raw JSON 使用体验；
- Building / Overview 页面布局；
- Elevation 输入体验；
- Sensor Layout 显示位置与细节；
- Validation 完整性；
- 少量 Metadata 和代码健壮性问题；
- QML 维护性和页面职责整理。

原则：

> 保持现有功能和架构稳定，以界面体验、健壮性和维护性优化为主。

---

## 2. Raw Metadata JSON 优化

当前 JSON 已支持滚动，但存在：

- 打开时默认定位到底部；
- 鼠标滚轮滚动速度偏慢。

修改要求：

1. 打开 Raw JSON Dialog 时始终定位到顶部；
2. 设置：
   - `cursorPosition = 0`
   - scroll / contentY = 0
3. 如有必要，在 Dialog 完成布局后使用延迟调用重新定位顶部；
4. 适当提高普通鼠标滚轮的滚动距离；
5. 保持触控板等高精度滚动输入自然；
6. JSON TextArea 与底部 `Format / Apply / Close` 按钮区分离：
   - JSON 区域滚动；
   - 按钮固定在底部。

---

## 3. Elevation 输入区域优化

当前 Elevation 输入框高度固定，Elevation 数量较多时内容会被遮挡。

修改为：

```text
ScrollView
    └── TextArea
```

要求：

- 输入框保持有限高度；
- 内容较多时出现纵向滚动条；
- 不因 Elevation 数量增加无限撑高 Building 页面；
- `ElevationNum / Parsed levels` 保持在滚动区域外部可见。

现有：

- 逗号；
- 空格；
- 分号；
- 换行

等输入格式继续保留。

---

## 4. Sensor Layout 居中

当前 2.5D Sensor Layout 已基本实现，但建筑图形整体偏左。

原因是当前缩放后仍从固定 `padding` 开始绘制，没有根据实际绘图尺寸计算剩余空间。

修改投影到屏幕后的 View Transform：

```text
drawWidth  = spanX * scale
drawHeight = spanY * scale

offsetX = (canvasWidth  - drawWidth)  / 2
offsetY = (canvasHeight - drawHeight) / 2
```

所有绘制坐标统一使用：

```text
screenX = offsetX + ...
screenY = ...
```

要求：

- 建筑线框在 Canvas 内水平、垂直居中；
- 不论由宽度还是高度限制 scale，都保持居中；
- Sensor marker、方向箭头、建筑边线使用同一套变换；
- Sensor 点击 hit-test 也必须使用完全相同的 View Transform。

避免：

```text
drawing transform
```

和：

```text
hit-test transform
```

分别维护。

---

## 5. Sensor Layout 小型视觉优化

在现有 2.5D 方案基础上适度优化，不重新改变技术路线。

建议：

- Sensor 方向线增加简单箭头头部；
- Selected Sensor 更明显；
- X/Y/Z/N 坐标方向显示更清楚；
- 坐标方向标识尽量放在视图角落，避免遮挡建筑；
- 继续支持 Rectangular / Circular / Polygon；
- 保持 XYZ 真实参与投影。

暂不要求：

- Qt Quick 3D；
- 自由旋转；
- 复杂实体模型。

---

## 6. Overview 页面重新整理

当前 Overview 信息量较少，宽屏下存在较大空白，同时底部 Building / Channels / Data / Validation 按钮与顶部 Tab 重复。

本轮：

### 删除

Overview 底部：

```text
Building
Channels
Data
Validation
```

跳转按钮。

顶部 Tab 已提供相同导航能力。

### 优化页面结构

建议顶部突出：

```text
Project Name
Document State
qREST Version
```

并增加关键摘要：

```text
Structural Type
Footprint
Elevation Count
Channel Count
Provider
Sampling Rate
NPTS
Duration
Event Name
Validation Status
Distance Unit
```

其中建议增加：

```text
Duration = NPTS / SamplingRate
```

以更直观表达数据长度。

### 视觉层级

适当增加：

- Project Name 字号；
- 关键数字字号；
- 页面标题与摘要间距。

Overview 应更接近：

> 文件 Dashboard / Summary

而不是普通 Metadata 字段列表。

---

## 7. Overview 文件信息调整

当前 Validation 区域中包含 Binary Summary，不太符合页面语义。

建议将其改为独立的 File / Format 摘要，例如：

```text
File Name
qREST Version
Document Mode
File Size
```

不要把：

```text
Metadata bytes
Packet bytes
```

作为 Validation 的主要展示内容。

详细二进制信息仍保留在 Advanced 中。

---

## 8. Overview / Building 页面整体居中

当前两页内容较少时会向左铺满，右侧出现明显空白。

建议建立统一的内容最大宽度：

```text
maxContentWidth ≈ 900 ~ 1050 px
```

页面逻辑：

```text
window narrow
    → content automatically shrinks

window wide
    → content width stops growing
    → entire content is horizontally centered
```

不要让普通表单横跨整个宽屏窗口。

---

## 9. Building 页面布局优化

Building 页面继续保持：

- Document；
- Building；
- Geo Location；
- Structural Footprint；
- Elevation。

但可以适度压缩布局。

宽屏时可以考虑部分 GroupBox 双列排列，例如：

```text
Document       Building
Geo Location   Elevation

Structural Footprint
```

不强制固定方案，目标是：

- 减少无意义的大面积横向空白；
- 保持字段输入舒适；
- 页面整体居中；
- 小窗口仍正常纵向滚动。

---

## 10. 抽取 BuildingPage.qml

当前：

```text
OverviewPage.qml
ChannelsPage.qml
DataPage.qml
ValidationPage.qml
```

已经独立。

Building 页面仍保留在 `main.qml`。

本轮建议新增：

```text
BuildingPage.qml
```

迁移：

- Units；
- Building；
- GeoLocation；
- StructuralFootprint；
- Elevation。

最终 `main.qml` 主要负责：

```text
ApplicationWindow
Menu
Toolbar
TabBar
StackLayout
Dialog wiring
```

降低后续维护成本。

---

## 11. Metadata DT 健壮性

当前 Metadata JSON 解析后会直接：

```text
Frequency = 1 / DT
```

如果 Raw JSON 中出现：

```text
DT = 0
```

或非有限值，会在 Validation 之前进入非法计算。

修改为：

```text
if DT > 0 and finite
    calculate Frequency
else
    Frequency = 0
```

是否合法继续交由 Validation 判断。

不要让非法 Raw JSON 导致不必要的计算异常。

---

## 12. Validation 完整性优化

当前已经统一使用共享 Core Validation，这一方向保持。

但 Final Validation 当前在 Metadata 已有 Error 时可能提前返回，导致：

- Packet channel count；
- Packet NPTS；
- sampling rate；
- timestamp；
- packet data size

等问题无法一次全部展示。

建议：

> 在安全的前提下继续执行彼此独立的检查。

只有确实依赖非法字段、无法安全计算的项目才跳过。

目标：

```text
一次 Validate
    ↓
尽可能完整展示当前文件所有问题
```

避免用户：

```text
修一个 → Validate → 再出现一个
```

---

## 13. Data 页面后续职责整理

本轮可视时间决定是否处理。

当前 Data 页面仍同时包含：

```text
Data Information
Packet Header
Data Matrix
```

而 Advanced 已经存在：

```text
Header / Packet Inspector
```

后续建议逐步将普通 Data 页面收敛为：

```text
Data Information
Data Matrix
```

Packet Header 等低层信息移入 Advanced。

本轮如改动较大会影响稳定性，可以暂不完全迁移，但应避免继续增加普通 Packet Header 编辑功能。

---

## 14. Format 常量集中

当前 Metadata Normalize 中：

```text
Header = qREST_DATA
Version = 1.0.0
```

存在局部写死。

本轮或后续建议将：

```text
Header
Version
```

放入统一 Schema / format 常量。

GUI 只读取统一定义。

避免未来格式版本升级时多个位置分别修改。

---

## 15. QrestTableView 滚动体验观察

当前公共 Table 组件已经基本解决 Header / Table 布局问题。

本轮无需主动大改。

但需要观察：

- 普通鼠标；
- 高分辨率滚轮；
- 触控板；

在不同平台上的滚动速度。

如果出现明显差异，再统一实现 Wheel Delta Normalization。

---

## 16. 本轮优先级

### P0

- Raw JSON 默认顶部；
- Raw JSON 滚动速度；
- Raw JSON 按钮区与滚动区分离；
- Elevation 输入 ScrollView；
- Sensor Layout 居中；
- Sensor 绘制与 hit-test 使用统一 Transform。

### P1

- Overview 页面重新设计；
- 删除 Overview 重复跳转按钮；
- 增加 Duration 等摘要信息；
- Overview / Building 最大内容宽度与居中；
- Building 布局优化；
- 抽取 `BuildingPage.qml`；
- Metadata DT 非法值安全处理；
- Final Validation 尽量返回完整问题。

### P2

- Sensor 箭头和坐标标识优化；
- Packet Header 进一步迁移到 Advanced；
- Format Header / Version 常量集中；
- TableView 不同输入设备滚动体验优化。

---

## 17. 本轮总体原则

当前版本已经具备主要功能，本轮重点不是继续增加功能数量，而是提升：

```text
界面可读性
+
使用舒适度
+
异常输入健壮性
+
代码可维护性
```

优先顺序：

```text
明确 Bug
    ↓
布局与交互
    ↓
Validation / Metadata 健壮性
    ↓
维护性
    ↓
视觉细节
```

尽量保持现有业务逻辑稳定，避免为单纯视觉调整重新修改已经成熟的数据层代码。