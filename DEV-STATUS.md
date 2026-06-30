# DEV-STATUS.md

## 当前状态

Agent 核已完成最小闭环，可在终端通过 TUI 界面运行。插件系统和安装脚本已就绪。

## 已完成

- 确定项目目标和文档约定，全部使用中文。
- 确定构建系统为 Xmake v3，C++23 标准，产物输出到 `bin/`。
- 引入依赖：`nlohmann_json`、`ftxui`、`boost_sml`、`taskflow`、`cpr`。
- 实现 `AFS_Config` 配置模块（JSON 加载，模型/TUI 配置读写）。
- 实现 `AFS_Model` / `AFS_Model_OpenAICompatible` 模型抽象层，支持 OpenAI 兼容 API（DeepSeek 等）。
- 实现 `AFS_Agent` 核心节点（树状结构、shared_ptr RAII）。
- 实现插件系统：`AFS::Plugin` 基类 + `AFS_PluginLoader` + 启动自动发现。
- 实现 `AFS_Loop` 对话循环（boost::sml 状态机）。
- 实现 `AFS_Context` 上下文管理与消息历史。
- 实现 `AFS_ToolRegistry` 工具注册与执行。
- 实现 `AFS_Logger` 日志输出（stderr + 内存缓冲）。
- 实现 FTXUI TUI 界面：
  - 状态栏（模型名、工作目录、上下文计数、Shell 模式指示）。
  - 消息区（动态折行、滚动、自动跟踪最新）。
  - 右侧栏（Quick Index 消息索引 + Files 文件浏览器，可拖拽调整宽度）。
  - 输入栏（Agent/Shell 模式切换、`@` 文件候选补全）。
  - 键盘映射子模块（keymap/，集中管理特殊键映射）。
- 实现控制台模式（`afs config.json "prompt"` 单次问答）。
- 实现 `install.sh` 一键安装脚本（核心需 root，插件和配置无需 root）。
- 实现 3 个工具插件：`compute`（加减乘除）、`bash`（Shell 命令执行）、`file`（文件读写）。
- 生成 `compile_commands.json` 辅助 LSP。

## 最近变更

- **2026-06-30**：修复消息区动态折行（`size(WIDTH)` 移到 `frame` 前）、移除侧边栏多余 `flex`、文件候选路径改为相对 cwd。
- **2026-06-30**：实现 `@` 文件候选补全：输入以 `@` 开头时显示文件候选，↑↓ 移动高亮，Tab 补全。
- **2026-06-29**：实现右侧栏 Files 文件浏览器（展开/折叠目录，可点击）。
- **2026-06-29**：新增 `file` 工具插件（file_read / file_write / file_exists）。

## 进行中

- 无。当前迭代已完成。

## 阻塞点

- 无。
