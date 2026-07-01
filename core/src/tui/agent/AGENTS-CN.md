# core > src > tui > agent > AGENTS-CN.md

TUI 的 Agent 核心交互层。负责把 TUI 前端和 Agent/Logger/Plugin/Model 基础设施隔离开。

## 文件

| 文件 | 职责 |
|------|------|
| `bridge.hh` | `AFS_TuiAgentBridge` 声明 |
| `bridge.cc` | 配置加载、插件加载、模型创建、Agent 运行、Logger 事件转 TUI 消息 |

## API

```cpp
class AFS_TuiAgentBridge {
  public:
    static std::unique_ptr<AFS_TuiAgentBridge> create(const std::string& config_path);

    const std::string& modelName() const;
    const std::string& workDir() const;
    bool running() const;
    std::size_t messageCount() const;

    bool submitUserMessage(const std::string& content);
    std::vector<TuiMessage> pollMessages();
};
```

## 流程

```
create(config)
  ├── AFS_Logger::init()
  ├── AFS_RegisterModelConfigSchemas() + AFS_ConfigManager::loadFromFile(config)
  ├── AFS_PluginManager::loadFromDirectory(AFS_DefaultPluginDirectory())
  ├── AFS_Agent::createMain()
  ├── agent->setModel(createModel(...))
  └── 记录 modelName / workDir

submitUserMessage(content)
  ├── 拒绝空输入或 running 状态
  ├── context.addMessage(UserMessage)
  └── 后台线程执行 agent->run()

pollMessages()
  ├── AFS_Logger::poll()
  └── AgentEvent → TuiMessage
```

## 事件转换约定

- `AgentEvent::AssistantMessage`：移除 `Message::print()` 生成的 `[assistant] ` 前缀，生成 `TuiMessage::Assistant`。
- `AgentEvent::AssistantDelta`：生成 `TuiMessage::Assistant`，`append = true`，由 `app.cc` 合并到上一条 Assistant。
- `AgentEvent::ReasoningMessage`：生成 `TuiMessage::Thinking`，用于 dim 思考段。
- `AgentEvent::ReasoningDelta`：生成 `TuiMessage::Thinking`，`append = true`，由 `app.cc` 合并到上一条 Thinking。
- `AgentEvent::ToolResult`：解析 `Message::print()` 生成的 `[tool call_id=...] ` 前缀：
  - `call_id=...` 写入 `TuiMessage::detail`；
  - `] ` 之后的 JSON / 输出写入 `TuiMessage::content`。
  - 若正文是 JSON，桥接层会先解析再展示；字符串字段中的 `\n`、`\t` 等转义显示为真实换行/制表效果。
- `AgentEvent::Error`：生成英文 `[error] ...` Assistant 消息。
- `AgentEvent::Complete`：生成 Assistant 消息。
- TUI 用户可见错误文案保持英文。

## Shell 模式边界

Shell 模式不经过本模块：`app.cc::submitShell()` 直接执行 `/bin/bash -lc`，只向 TUI `messages_` 追加 `TuiMessage::Shell`，不调用 `submitUserMessage()`，不写入 Agent 上下文。

## 约定

- 只暴露 TUI 所需的 Agent 状态和消息；不要把 `AFS_Agent` 指针泄漏到布局或输入模块。
- Logger 事件在这里转换成 `TuiMessage`，避免 `app.cc` 直接理解 `AgentEvent` 细节。
- 插件默认从 `${XDG_CONFIG_HOME:-~/.config}/afs/plugins/` 加载。
- 工作目录显示只保留最后两级，避免状态栏过长。
- 当前实现使用后台线程运行 `agent->run()`，`running()` 用 atomic 标记；提交时若正在运行则拒绝新输入。
