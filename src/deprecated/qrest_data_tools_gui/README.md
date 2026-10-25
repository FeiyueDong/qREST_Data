# 已废弃：qREST Data Tools GUI

2026-10-25 起，数据编辑界面由 `qrest_module` 维护，本目录仅作历史归档。
这里的源码、资源、构建文件、文档和脚本均不参与主项目的构建、安装或发布，
不再提供启用选项。历史命令和相对路径保留迁移前的含义，不作为当前使用说明。

- 原 `src/qrest_data_tools/gui/` 下的代码、QML、图标、帮助和构建文件整体保留。
- 原开发说明保存在 `doc/development.md`；原主项目说明和发布记录保存在
  `doc/project_readme.md`、`doc/release.md`。
- `scripts/release.py` 保存迁移前包含界面部署流程的发布脚本，仅供查阅。
- `doc/dev/` 和 `editor/` 保存当前检出中的历史规划及编辑器配置。

共享的读写、格式导入、验证及 `DatasetDraft` 核心继续由
`src/qrest_data_tools/core/` 维护。
