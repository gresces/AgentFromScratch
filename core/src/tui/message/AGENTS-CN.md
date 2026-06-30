# core > src > tui > message > AGENTS-CN.md

TUI 消息数据模型子模块。提供前端渲染所需的最小消息结构，不依赖 FTXUI，不访问 Agent。

## 文件

| 文件 | 职责 |
|------|------|
| `message.hh` | `TuiMessage` 结构体 |

## API

```cpp
struct TuiMessage {
    enum Role { User, Assistant, Thinking, Tool, Shell };
    Role role;
    std::string content;
    std::string detail;
    bool append = false;
};
```

## 字段语义

- `role`：只描述 TUI 展示角色，不等同于 `AFS::Role` 的完整语义。
- `content`：已经准备展示的正文；布局模块不应重新解析 Agent 事件。
- `detail`：显示在 role header 中的短元数据，例如 `call_id=call_xxx`、`command`、`exit_code=0`。
- `append`：流式 delta 标记；为 true 时 `app.cc` 会把正文追加到上一条同 role/detail 消息。

## Role 约定

| Role | 用途 | Header 示例 |
|------|------|-------------|
| `User` | 用户提交给 Agent 的消息 | `-- user ----` |
| `Assistant` | Agent 回复 / 错误 | `-- assistant ----` |
| `Thinking` | 模型思考过程 / reasoning 内容，正文使用 dim 样式 | `-- thinking ----` |
| `Tool` | Agent 工具结果 | `-- tool call_id=call_xxx ----` |
| `Shell` | TUI-only shell 模式命令和输出 | `-- shell exit_code=0 ----` |

## 约定

- Assistant `content` 不带 `[assistant]` 前缀。
- Tool `content` 不带 `[tool call_id=...]` 前缀；`call_id` 存入 `detail`。
- Thinking `content` 只显示可展示的模型 reasoning/thinking 内容，布局层整体 dim，不进入用户输入。
- Shell 消息不写入 Agent 上下文，只用于 TUI 显示。
- 新增角色或字段时同步更新：
  - `layout/layout.cc` 的 role header 颜色；
  - `agent/bridge.cc` 的 `AgentEvent` 转换；
  - `app.cc` 的 TUI-only 消息创建；
  - `core/src/tui/AGENTS-CN.md` 和本文件。
