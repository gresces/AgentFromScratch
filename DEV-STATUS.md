# DEV-STATUS.md

## 当前状态

项目处于初始化阶段，正在建立顶层说明文件和后续开发任务。

## 已完成

- 明确项目目标：使用 C++ 构建高性能、可拓展的个人 Agent。
- 明确首要交付物：可在终端运行的 Agent 核二进制程序。
- 明确文档约定：描述性文件全部使用中文。
- 确定构建系统为 Xmake v3，项目入口 `core/xmake.lua`。
- 确定使用 C++23 标准。
- 确定编译产物统一输出到仓库顶层 `bin/` 目录。
- 引入 `nlohmann_json` 和 `ftxui` 作为项目依赖。
- 引入 `boost_sml` 作为 Agent 核心状态机（状态转移）。
- 引入 `taskflow` 作为状态内部任务并行器件。
- 引入 `cpr` 作为 HTTP 网络请求库。
- 实现 `AFS_Config` 配置模块（JSON 加载）。
- 实现 `AFS_Model` / `AFS_Model_OpenAICompatible` 模型抽象层。
- 实现 `AFS_Model_DeepSeek` DeepSeek API 模型。
- 生成 `compile_commands.json` 辅助 LSP 静态分析。
- 实现 `AFS_Agent` Agent 核心节点（树状结构、shared_ptr RAII）。
- 实现插件系统：`AFS_Plugin` 抽象基类 + `AFS_LoadedPlugin` RAII + `AFS_PluginLoader`。

## 进行中

- 实现 Agent 核的最小闭环：对话循环、工具调用、上下文管理。
