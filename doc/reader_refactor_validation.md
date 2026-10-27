# 外部格式读取重构验证记录

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
