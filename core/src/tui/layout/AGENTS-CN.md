# core > src > tui > layout > AGENTS-CN.md

TUI 布局与样式子模块。负责把 UI 状态渲染为 FTXUI `Element` / `InputOption`，不处理事件，不访问 Agent。

## 文件

| 文件 | 职责 |
|------|------|
| `layout.hh` | 布局渲染 API 与 `AFS_TuiStatusView` |
| `layout.cc` | 状态栏、消息区、输入栏样式实现 |

## API

```cpp
struct AFS_TuiStatusView {
    bool esc_pending;
    bool agent_running;
    bool shell_running;
    bool shell_mode;
    int spinner_frame;
    std::string model_name;
    std::string work_dir;
    std::size_t context_count;
};

ftxui::InputOption AFS_TuiInputOption();
ftxui::Element AFS_TuiRenderStatus(const AFS_TuiStatusView& view);
ftxui::Element AFS_TuiRenderMessages(const std::vector<TuiMessage>& messages);
ftxui::Element AFS_TuiRenderInput(ftxui::Component input_component, bool shell_mode);
```

## 用户可见文案

TUI 默认使用英文文案：`Ready`、`Agent running ...`、`Shell ready`、`Shell running ...`、`Press Esc again to exit`、`Type a message to start ...`。

## 样式约定

状态栏：单行显示。左侧显示 ready/running + spinner；右侧显示模式（`AGENT` / `SHELL`）、模型名、工作目录、上下文条数。元素之间用 `|` 连接。
- `esc_pending == true` 时状态栏只显示黄色 `Press Esc again to exit`。
- 消息区按 `TuiMessage::Role` 着色：
  - `User`：绿色
  - `Assistant`：青色
  - `Tool`：黄色
  - `Shell`：品红
- 消息 role header 使用 `-- role detail ----` 形式；`detail` 为空时省略。
- 只有 role 名称使用角色颜色和加粗；`detail`（例如 `call_id=...`）使用 `dim`，不要突出显示。
- Tool 消息的 `call_id=...` 放在 header 的 `detail` 中，正文只显示 JSON 或输出内容。
- Assistant 正文不显示 `[assistant]` 前缀。
- 内容使用 `paragraph()` 自动换行。
- 输入栏通过 `InputOption::transform` 去除反转背景，只使用 `dim`；不要加 `underlined`。
- 输入提示符：Agent 模式为 `>`，Shell 模式为 `$`。

## 边界

- 不修改 `scroll_offset_`；滚动由 `input/` 和 `app.cc` 协调。
- 不调用 `AFS_Logger` / `AFS_Agent`。
- 不创建线程。
