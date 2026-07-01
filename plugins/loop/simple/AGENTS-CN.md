# plugins > loop > simple > AGENTS-CN.md

Loop 运行时插件，提供 Agent 默认对话循环实现。

## 文件

| 文件 | 职责 |
|------|------|
| `loop.cpp` | `AFS::Loop` 默认实现与 `LoopPluginSimple` 插件入口 |
| `xmake.lua` | 插件 xmake 构建定义，使用 xmake 包管理 `boost_sml`、`nlohmann_json` |
| `build.sh` | 兼容顶层 `plugins/build.sh` 的构建/安装/清理包装脚本 |

## 功能

- 构建 LLM chat/completions 请求。
- 优先使用流式响应，失败且未输出 delta 时回退普通请求。
- 解析 assistant 文本、reasoning 与 tool_calls。
- 调用 `AFS::ToolExecutor` 执行工具，并向 Context 写入 Tool 消息。
- 通过 `AFS::LoopEvents` 发布 TUI/控制台运行时事件。

## 安装路径

构建产物为 `LoopPluginSimple`，安装到：

```sh
${XDG_CONFIG_HOME:-~/.config}/afs/plugins/loop/LoopPluginSimple
```
