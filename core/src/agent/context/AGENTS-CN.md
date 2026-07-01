# core > src > agent > context > AGENTS-CN.md

Agent 上下文接口兼容层。默认实现位于 `plugins/context/` 目录下（代码按类型自动发现第一个已加载插件，不依赖具体名称），本目录只保留内部 include 路径兼容。

## 文件

| 文件 | 职责 |
|------|------|
| `context.hh` | 包含 `<afs/context.hh>`，并提供 `using AFS_Context = AFS::Context` |

## `AFS_Context`

`AFS_Context` 是 `AFS::Context` 的别名。每个 `AFS_Agent` 通过插件管理器创建一个 `ContextPluginSimple` 实例，并以 `std::unique_ptr<AFS_Context>` 独占持有。

| 方法 | 说明 |
|------|------|
| `addMessage(msg)` | 添加单条消息到上下文末尾 |
| `addMessages(msgs)` | 批量添加消息 |
| `messages()` | 获取所有消息的只读引用 |
| `messageCount()` | 当前消息数量 |
| `buildRequest()` | 构建 LLM 请求用的消息数组 |
| `buildPrompt()` | 构建可读的上下文摘要字符串（每条消息一行） |
| `clear()` | 清空全部消息（同时归零 token 计数） |
| `setTokenCounter(fn)` | 设置 token 计数回调 |
| `recomputeTokens(fn)` | 使用回调重新计算全部消息 token 总数 |
| `tokenCount()` | 当前累计 token 数 |

## 与 `AFS_Agent` 的关系

- `AFS_Agent` 构造时调用 `AFS_PluginManager::createContext()` 创建上下文实例。
- `context()` 返回接口引用，TUI 和控制台模式通过它追加用户消息、读取消息数和 token 数。
- 默认系统提示词仍由 `AFS_Agent::initDefaultContext()` 追加。
- 工具提示词仍由 `AFS_Agent::registerTools()` / `loadExtraTool()` 追加。

## 实现位置

默认实现文件：`plugins/context/<name>/context.cpp`（例如 `plugins/context/simple/context.cpp`）。

构建安装：

```sh
cd plugins
./build.sh context install
```
