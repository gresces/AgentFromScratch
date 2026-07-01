# core > src > agent > loop > AGENTS-CN.md

Agent 核心运行循环接口兼容层。默认状态机实现位于 `plugins/loop/` 目录下（代码按类型自动发现第一个已加载插件，不依赖具体名称），本目录保留接口别名和 `agent.loop` 配置加载。

## 文件

| 文件 | 职责 |
|------|------|
| `loop.hh` | 包含 `<afs/loop.hh>`，提供 `AFS_Loop` / `AFS_AgentLoopConfig` 兼容别名与配置函数声明 |
| `loop.cc` | `AFS::LoopConfig` schema、JSON 解析与 `AFS_LoadAgentLoopConfig()` 实现 |

## `AFS_Loop`

`AFS_Loop` 是 `AFS::Loop` 的别名。默认实例由 `LoopPluginSimple` 创建：

```cpp
std::string run(AFS::Context& context, AFS::ToolExecutor& tools,
                const AFS::Model& model, AFS::LoopEvents& events,
                const std::string& agent_uuid);
```

运行完整循环，仅依赖五个精确资源，不依赖整个 Agent。运行时事件通过 `AFS::LoopEvents` 交回宿主，宿主再写入 `AFS_Logger`。

## 配置

`AFS::LoopConfig` 当前字段：

| 字段 | 默认值 | 说明 |
|------|--------|------|
| `max_iterations` | `50` | 最大工具调用循环次数，防止无限循环 |

配置路径：

```json
{"agent":{"loop":{"max_iterations":50}}}
```

## 默认实现位置

默认实现文件：`plugins/loop/<name>/loop.cpp`（例如 `plugins/loop/simple/loop.cpp`）。

该插件使用 xmake 包管理依赖：`boost_sml`、`nlohmann_json`。

构建安装：

```sh
cd plugins
./build.sh loop install
```

## 职责边界

| 组件 | 职责 |
|------|------|
| `AFS_Agent` | 持有 Loop、Context、Model；通过 `run()` 传递四个资源给 Loop |
| `LoopPluginSimple` | 状态机：WaitingLLM ↔ Executing，调度 LLM 调用和工具执行 |
| `ContextPluginSimple` | 消息存储：累积对话历史 |
| `AFS_Model` | LLM 调用：`chatCompletionStream` / `chatCompletion` |

## 约束

- 创建 Agent 前必须已加载 `LoopPluginSimple`。
- `AFS_Loop::run()` 仅接收精确依赖：`Context&`、`ToolExecutor&`、`const Model&`、`LoopEvents&`、`uuid`。
- 运行时事件仅写入 `AFS::LoopEvents`，不感知 `AFS_Logger` 或订阅者。
- 默认实现仍保留 DeepSeek `reasoning_content` 回写逻辑，带工具结果的后续请求必须保留该字段。
