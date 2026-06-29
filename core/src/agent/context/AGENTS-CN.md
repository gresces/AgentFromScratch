# core > src > agent > context > AGENTS-CN.md

Agent 上下文管理器。每个 `AFS_Agent` 持有一个实例，负责累积对话历史并构建 LLM 请求。

## 文件

| 文件 | 职责 |
|------|------|
| `context.hh` | `AFS_Context` 类声明 |
| `context.cc` | 实现 |

## `AFS_Context`

`AFS_Context` 是 Agent 的记忆核心，以 `AFS::Message` 序列的形式维护对话历史。
消息按添加顺序排列，支持向 LLM 输出完整请求或可读摘要。

| 方法 | 说明 |
|------|------|
| `addMessage(msg)` | 添加单条消息到上下文末尾 |
| `addMessages(msgs)` | 批量添加消息 |
| `messages()` | 获取所有消息的只读引用 |
| `messageCount()` | 当前消息数量 |
| `buildRequest()` | 构建 LLM 请求用的消息数组 |
| `buildPrompt()` | 构建可读的上下文摘要字符串（每条消息一行） |
| `clear()` | 清空全部消息 |

## 与 `AFS_Agent` 的关系

- 每个 `AFS_Agent` 通过 `context()` 访问其上下文。
- 所有 Agent 构造时自动初始化默认上下文。消息顺序如下：

```
### 上下文消息顺序

1. [System]    You are a helpful assistant. Use tools when appropriate.
2. [Developer] You have access to the following tools:
               - compute: Binary arithmetic ... (input: {...})
               - weather: Get weather ... (input: {...})
3. [User]      What is 2+2?
4. [Assistant] 4
5. [Tool]      {"result": 8} (call_id=tc_001)
```

- 第 1 条由构造函数通过 `initDefaultContext()` 自动添加（系统提示词）。
 第 2 条由 `createMain()` 自动调用 `registerTools()` 追加（主 Agent 构造时）。
 后续子 Agent 可通过 `loadExtraTool()` 追加更多工具提示。
- 后续消息由 Agent 运行时逻辑添加。

## 使用示例

```cpp
auto main = AFS_Agent::createMain();
// 构造时自动完成: [System] + [Developer] (工具列表)

// 用户输入
main->context().addMessage(AFS::UserMessage("What is 2+2?"));

// 构建 LLM 请求
auto request = main->context().buildRequest();
// 发送给 LLM → 获得响应

// 记录 LLM 响应
main->context().addMessage(AFS::AssistantMessage("4"));

// 查看上下文摘要
std::cout << main->context().buildPrompt();
```

## 上下文生命周期

```
Agent 构造 → initDefaultContext()         → [System]
createMain() 自动调用 registerTools()      → [Developer] (工具列表)
运行时    → context().addMessage(User)    → [User]
          → LLM 响应                      → [Assistant]
          → 工具调用                      → [Tool]
子 Agent  → 构造时同样获得默认上下文       → [System]
          → 可继续追加消息
Agent 析构 → 上下文随 Agent 一起销毁
```

## `buildPrompt()` 输出示例

```
[System] You are a helpful assistant. Use tools when appropriate.
[Developer] You have access to the following tools:
- compute: Binary arithmetic: add, sub, mul, div. Input: {"op":"add","a":1,"b":2} (input: {"type":"object","properties":{"op":{"type":"string","enum":["add","sub","mul","div"]},"a":{"type":"number"},"b":{"type":"number"}},"required":["op","a","b"]})
[User] What is 2+2?
[Assistant] 4
```
