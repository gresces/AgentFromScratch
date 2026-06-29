# core > src > tui > input > AGENTS-CN.md

TUI 输入事件处理子模块。负责把 FTXUI `Event` 转成可执行的 UI 动作，不负责渲染，不直接访问 Agent。

## 文件

| 文件 | 职责 |
|------|------|
| `input.hh` | 输入事件 helper 声明 |
| `input.cc` | modified Enter、Esc 确认取消、滚动事件处理 |

## API

```cpp
bool AFS_TuiIsMultilineShortcut(const ftxui::Event& event);
bool AFS_TuiCancelsExitConfirmation(const ftxui::Event& event);
bool AFS_TuiHandleScrollEvent(ftxui::Event event, int total_messages, int& scroll_offset);
```

## 快捷键约定

- `Enter` 不在本模块匹配为换行；由 `app.cc` 按当前模式处理：
  - Agent 模式：提交给 Agent；
  - Shell 模式：执行当前输入的 bash 命令。
- `Tab` 由 `app.cc` 消费，用于在 Agent 模式和 Shell 模式之间切换；本模块只把它视为会取消 Esc 退出确认的普通输入事件。
- 多行输入只匹配 modified Enter 序列：
  - `ESC [ 13 ; 2 u`：Shift+Enter
  - `ESC [ 13 ; 5 u`：Ctrl+Enter
  - `ESC [ 13 ; 6 u`：Ctrl+Shift+Enter
  - `ESC [ 27 ; 2 ; 13 ~`：legacy Shift+Enter
  - `ESC [ 27 ; 5 ; 13 ~`：legacy Ctrl+Enter
  - `ESC [ 27 ; 6 ; 13 ~`：legacy Ctrl+Shift+Enter
- 不要把 `Event::CtrlJ` 当换行快捷键；FTXUI 中 `Event::Return` 和 `Event::CtrlJ` 都是 LF，无法可靠区分 plain Enter 和 Ctrl+J。
- `Esc` 双击退出的状态由 `app.cc` 保存；本模块只提供“哪些事件会取消确认”的判断。
- 鼠标滚轮、`↑` / `↓`、`PageUp` / `PageDown` 更新 `scroll_offset`。

## Shell 模式边界

Shell 模式本身不在本模块实现；本模块只保证 `Tab` 不被误判为滚动或换行。执行逻辑在 `app.cc::submitShell()` 中。

## 边界

- 不读写 `messages_`、`input_`、`agent_bridge_`。
- 不调用 FTXUI 渲染 API。
- 不直接调用 `screen.Exit()`；退出确认由 `app.cc` 协调。
