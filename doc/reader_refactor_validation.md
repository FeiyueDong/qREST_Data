# MiniSEED 时间精度修复验证记录（2026-10-27）

依据：本地 `doc/dev/qrest_data_reader_fix_plan_v1_0.md`；审查基线
`1684419f1188603cd9284b7f75ef7abf11972c0f`。本轮在主工作区
`/home/yue/workspace/CodeFiles/qrest_data` 实施，未使用其他 worktree。

结论：**M1、M2 及 Linux 功能基线完成；M3 Windows/MSVC 待验收；M4 已测量并记录。**
模块版本维持 `1.1.1`，本轮不正式发版；`.qrest`、metadata、DataPacket 协议与
公开 `qrest_external_*` C ABI 均保持。

## 修复与复现

用测试内独立生成的合法 MiniSEED 2 + Steim-2 记录（每记录 100 个递增整数）
复现修复前错误：75 Hz 第二条记录被报为不在采样网格，残差约 `33.3333 us`。
现已加入 BTime（100 us）/ Blockette 1001（1 us）精度来源；按参考记录与当前
记录的编码步长计算 `(q1+q2)/2` 误差预算，并加相对时间运算的浮点误差保护。
完整容差必须严格小于半个采样间隔，否则明确拒绝。

时间检查同时使用首条记录确定的共同采样网格、相邻记录约束、所有记录共同
允许的量化相位区间，跨通道对齐复用相同原则。相位区间只用于完整性检查，
不修改来源时间戳、波形值、采样率或物理单位；闭合边界保留 halfway / ties-to-even
编码的合法情况。计算先做整数时间差，再换算相对微秒，避免大 Unix 时间相减
以及理论记录时长逐条累计误差。保留 `FillNaN`、`Error`、显式 `Ignore` 边界。

新增精度测试覆盖 75/120 Hz 多条非整数时长记录、BTime 舍入/截断、128/512 Hz
halfway ties-to-even、Blockette 100 实际频率优先于固定因子（75.5/120.125 Hz）、
1001 正负微秒偏移与异常、两种 Blockette 共存及非法重复、缺一整条/一个样本、
重叠、乱序、跨通道首尾对齐、渐进漂移及 5/10 kHz、500 kHz 精度不可判定情形。
同时检查数值、点数、首末时间、缺失统计与 C ABI qREST round-trip。

## 本轮 Linux 执行证据

环境：Linux x86_64（Arch，内核 `7.2.11-arch1-1`）；Intel i7-9700K，
系统可见内存约 32 GiB；GCC `16.2.1 20260810`，Xmake `3.1.2+20261016`，
系统 HDF5 `2.2.0`。Xmake 使用临时全局目录 `/tmp/qrest_reader_xmake`。

```sh
XMAKE_GLOBALDIR=/tmp/qrest_reader_xmake XMAKE_SKIP_HISTORY=y \
    xmake config -p linux -m release --policies=package.fetch_only -y
# 先完成聚焦目标与新时间精度测试，再执行全量构建：
xmake build test_qrest_data_format_readers qrest_data_tools_cli
xmake build
xmake run test_qrest_data_format_readers
xmake run test_qrest_data_format_readers --full
xmake run test_qrest_data_import_formats
xmake run test_qrest_data_lib
xmake run test_qrest_data_c_api
xmake run test_qrest_data_hdf5
xmake run test_qrest_data_dataset_draft
python3 scripts/test_format_reader_cli.py build/linux/x86_64/release/qrest_data_tools_cli
```

实际运行时上述 Xmake 命令均使用同一 `XMAKE_GLOBALDIR` / `XMAKE_SKIP_HISTORY`。
结果：Release 全量构建成功；六个测试目标全部通过；常规格式读取测试 **143 项**，
包含完整文件后 **10957 项**；DatasetDraft **68 项**；CLI **29 项**。
C ABI fractional-rate round-trip 和 CLI 提取出的 `DT`、`NPTS`、`StartTime`、单位、
`Scale` 均有断言。

本地完整三文件 SHA-256 和大小均重新计算，与
[`sources.json`](../resource/test/format_readers/sources.json) 一致。
再次确认 **1080000 个整数完全相同、10800 条完整压缩数据区匹配**；
DAT → Gal 与 TXT 的最大差仍为 **`2.77556e-17 Gal`**，满足 `1e-12` 门限。
原始方向顺序和 `14:00 UTC = 22:00+08:00` 不变。武汉 Modified MiniSEED、
既有 TDMS、HDF5 的原有数值及误差阈值均通过；未适配新的 TDMS 文件。

## SDK、符号与发布包

```sh
XMAKE_GLOBALDIR=/tmp/qrest_reader_xmake XMAKE_SKIP_HISTORY=y \
    xmake install --packages=n -o /tmp/qrest_reader_fix_sdk \
    qrest_data_headers qrest_data_lib qrest_data_readers
```

把 `tests/installed_sdk_smoke` 的 `main.cpp`、`reader_c.c`、`xmake.lua` 复制到
源码树外 `/tmp/qrest_reader_fix_smoke`，仅配置安装前缀：

```sh
xmake config -P /tmp/qrest_reader_fix_smoke -p linux -m release \
    --qrest_data_sdk=/tmp/qrest_reader_fix_sdk -y
xmake build -P /tmp/qrest_reader_fix_smoke
```

独立纯 C 与 C++ 程序均编译、链接并实际读取复制到该目录的 1 s DAT 成功；
无参数错误处理也通过。`nm -D` 实际导出的 8 个 `qrest_external_*` 符号与
基线公开头文件声明集合一致；Linux `ldd` 确认 reader 依赖系统 HDF5，无 Qt。

Linux 发布目录通过 `release.py linux --mode release` 生成，随后使用包内 CLI
再次执行 29 项端到端测试。公开头文件、reader 动态库、原 C ABI 库、CLI 文件完整。
包内 `bin/` 只有 CLI。上述为 Linux 实际验证，不能替代 Windows DLL 部署。

## 性能基线（单次测量）

```sh
python3 scripts/benchmark_format_reader.py \
    build/linux/x86_64/release/qrest_data_tools_cli \
    --work-dir /tmp/qrest_reader_fix_benchmark --channels 63 --rate 200 --seconds 3600
```

重复项目自有 fixture 的合法 Steim-2 载荷，生成独立源通道，按时间字段写出
合成输入。**63 通道 × 720000 点/通道 = 45360000 点**，输入 **232243200 B**，
一份 double 时程 **362880000 B**，输出 qREST **362889663 B**。
输入 SHA-256：`914f2a0bb9d2b245d00dbad713f64318d9c229c0396f4dd3ee255360511d3a1a`。

| 操作 | 整进程墙钟耗时 | 峰值 RSS（KiB） | 峰值 RSS（GiB） | 退出码 |
| --- | --- | --- | --- | --- |
| validate | 1.748505 s | 1205944 | 1.1501 | 0 |
| import（RawCounts、double 编码） | 4.053642 s | 1964612 | 1.8736 | 0 |

通过 Linux `wait4` 获取每个 CLI 子进程的峰值 RSS，单独记录 stdout/stderr；
生成、哈希计算和生成后的额外 `validate qrest` 不计入表中耗时。大文件 qREST
验证也已通过。原始测量、命令日志和输出位于该 scratch 目录。
这是单机、单次、生成后文件缓存条件下的整体进程基线，不能作为跨硬件性能承诺；
不单独声称解析/组装/映射各阶段的峰值，也不代表真实复杂波形压缩性能。
峰值明显高于单数组容量，本轮据实记录多份数据副本的成本，未做流式化或并行重构。

## Windows/MSVC 待验收与限制

**M3 未完成，原因属于执行环境不可用**：本次实际主机是 Linux；未提供
Windows/MSVC 执行环境，`cl`、PowerShell、Wine 均不可用。
因此没有 Windows Release 编译、DLL/导入库运行、HDF5 依赖部署、独立 SDK
实际消费或干净机器验证结论；MinGW 和远程 CI 也未执行。

Windows 环境需按方案 M3 执行 Release 构建、上述格式/CLI 测试、
`release.py windows --mode release`，检查 reader DLL、导入库、公开头文件、
CLI 及全部 HDF5 依赖，再在源码树外进行纯 C/C++ SDK 和干净环境运行。
Linux 功能修复通过不等于跨平台正式发布验收完成。

`git diff --check` 与 Python 语法检查通过。未改变格式集合、公开 C ABI、物理量
或灵敏度规则；以下保留首轮重构的历史记录，检查数量属于当时的基线。

---

# 首轮外部格式读取重构验证记录

日期：2026-10-27。工作区：`/home/yue/workspace/CodeFiles/qrest_data`（主工作区）。
实施依据：本地 `doc/dev/qrest_data_format_reader_refactor_v1_0.md`。
使用方法与支持边界见 [工具说明](qrest_data_tools.md)。

本次新增标准 MiniSEED 2、SIT-4S DAT、SIT Gal TXT 的共用读取与转换路径、
CLI `import/validate` 和独立 `qrest_data_readers` C ABI 动态库。
用户已有的 `.gitignore` 修改保留；完整原始文件继续位于被忽略的
`resource/example_data`。该目录中的新 TDMS 文件未纳入本轮适配。

## 全量数值与时间验证

| 检查 | 结果 |
| --- | --- |
| DAT / MiniSEED 原始整数 | 3 × 360000 = 1080000 个样本逐点完全一致 |
| 二进制对应压缩载荷 | 10800 / 10800 条记录匹配（每条比较 DAT 的完整 192 B 数据区） |
| DAT → Gal / TXT | 全量比较最大绝对误差 `2.77556e-17 Gal`，小于 `1e-12` |
| 二进制顺序 | EIN / EIE / EIZ，即 NS / EW / UD |
| TXT 顺序 | UD / NS / EW，保持源顺序，通过显式映射对齐 |
| 首值，NS / EW / UD | `0.019 / -0.008 / 0.008 Gal` |
| 每通道时程 | 360000 点，100 Hz，dt = 0.01 s |
| 起点 | `2026-10-22T14:00:00Z` = TXT `22:00:00+08:00` |
| 末采样 | UTC `14:59:59.990` = TXT `22:59:59.990+08:00` |
| 完整回归测试 | `test_qrest_data_format_readers --full` 通过 10884 项检查 |

最小 1 s fixture、独立测试元数据与原始文件 SHA-256 保存在
`resource/test/format_readers`，无需完整本地文件也可执行常规回归。

## Linux/GCC 已执行

- 显式配置 Linux Release，聚焦目标构建后执行 `xmake build`，完成。
- 六个测试目标全部通过：`test_qrest_data_lib`、`test_qrest_data_c_api`、
  `test_qrest_data_hdf5`、`test_qrest_data_import_formats`、
  `test_qrest_data_dataset_draft`（68 项）、`test_qrest_data_format_readers`。
- 武汉 MiniSEED 原有采样数、首末数值及数值误差阈值保持；测试 metadata
  占位时间改为源文件实际起点。双仪器目录测试改为不同站点标识，避免把同一
  文件的重复导出当成不同仪器；波形载荷不变。旧武汉 TDMS / HDF5 回归通过。
- 异常测试包含缺校准、未知编码、变长记录/Blockette 位置、端序、截断、
  末值常数、积分溢出、时间修正、缺口/重叠/非采样网格、损坏 DAT、
  GBK/UTF-8/BOM、TXT 行号/时间错误，以及 metadata 单位/量纲/起点/Scale/映射冲突。
- `scripts/test_format_reader_cli.py` 21 项 CLI 端到端检查通过：三格式
  导入、qREST 验证/提取、显式映射、原始计数模式与错误输入拒绝。
- 独立 `tests/products_smoke` 构建及运行通过；新增 reader target 在产品入口
  可构建，入口未引入测试 target，模块版本保持独立。
- 安装前缀 `/tmp/qrest_reader_sdk` 的独立 C++ SDK 和纯 C reader 消费工程
  编译、链接，并实际读取 1 s DAT 成功，未引用源码/include/build 目录。
- `nm -D` 确认 8 个 `qrest_external_*` C 导出；Linux 运行依赖包含系统 HDF5，
  无 Qt 依赖。
- `release.py linux --mode release --output-dir /tmp/qrest_reader_release`
  生成发布目录，包含 reader 动态库与公开头文件；`bin/` 只有 CLI。
  发布包 CLI 再次通过同一组 21 项端到端检查。
- `git diff --check` 通过。

本地 Xmake 全局缓存隔离到 `/tmp/qrest_reader_xmake`，以复用系统依赖并避免
修改用户全局缓存；这不改变仓库的 Xmake 构建入口。

## 尚未执行的验收

Windows/MSVC 构建、Windows DLL 部署及干净机器运行、MinGW 开发构建和远程 CI
未在此 Linux 环境执行，仍是相应平台的验收项。标准 MiniSEED 非 Steim-2 编码、
MiniSEED 3、其他 DAT/TXT profile 不在当前支持范围，读取器明确拒绝。
