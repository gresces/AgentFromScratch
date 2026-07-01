# core > src > tui > input > AGENTS-CN.md

TUI 输入事件处理子模块。负责把 FTXUI `Event` 转成可执行的 UI 动作，不负责渲染，不直接访问 Agent。

## 文件

| 文件 | 职责 |
|------|------|
| `input.hh` | 输入事件 helper 声明，暴露 `AFS_TuiKeyActionEvent` |
| `input.cc` | 根据 `keymap/` 表解析特殊键，处理 modified Enter、Esc 确认取消、滚动事件 |

## API

```cpp
std::optional<AFS_TuiKeyActionEvent> AFS_TuiResolveKeyAction(const ftxui::Event& event);
bool AFS_TuiIsMultilineShortcut(const ftxui::Event& event);
bool AFS_TuiCancelsExitConfirmation(const ftxui::Event& event);
bool AFS_TuiHandleReadlineShortcut(const ftxui::Event& event, std::string& input,
                                   int& cursor_position);
bool AFS_TuiHandleScrollEvent(ftxui::Event event, int& scroll_position, bool& follow_latest);
```

## 快捷键约定

特殊键映射不散落在 `input.cc` 中；`input.cc` 只扫描 `keymap/keymap.hh` 的 `AFS_TuiKeyBindings` 表并返回语义动作事件。

- `Enter` 不在本模块匹配为换行；由 `app.cc` 按当前模式处理：
  - Agent 模式：提交给 Agent；
  - Shell 模式：执行当前输入的 bash 命令。
- `Tab` 默认由 `app.cc` 消费用于在 Agent/Shell 模式之间切换；当 Agent 模式输入以 `@` 开头并显示文件候选时，`app.cc` 优先把 `Tab` 用作候选补全。
- 多行输入只匹配 modified Enter 序列：
  - `ESC [ 13 ; 2 u`：Shift+Enter
  - `ESC [ 13 ; 5 u`：Ctrl+Enter
  - `ESC [ 13 ; 6 u`：Ctrl+Shift+Enter
  - `ESC [ 27 ; 2 ; 13 ~`：legacy Shift+Enter
  - `ESC [ 27 ; 5 ; 13 ~`：legacy Ctrl+Enter
  - `ESC [ 27 ; 6 ; 13 ~`：legacy Ctrl+Shift+Enter
- 不要把 `Event::CtrlJ` 当换行快捷键；FTXUI 中 `Event::Return` 和 `Event::CtrlJ` 都是 LF，无法可靠区分 plain Enter 和 Ctrl+J。
- `Esc` 双击退出的状态由 `app.cc` 保存；映射表只把 `Esc` 解析为 `BeginExit` 动作。
- 鼠标滚轮、`↑` / `↓`、`PageUp` / `PageDown` 更新 0..1000 的相对 `scroll_position`。`input.cc::adjustScrollPosition` 只把增量裁剪到 0..1000 范围；实际的 80% 页面高度动态裁剪在 `app.cc` 的 `AFS_TuiKeyAction::Scroll` 处理器和鼠标滚轮处理器中完成。`@` 文件候选激活时，`app.cc` 优先把 `↑` / `↓` 用作候选高亮移动。
- Readline 风格编辑快捷键由 `AFS_TuiHandleReadlineShortcut()` 统一处理，调用方必须传入当前输入 buffer 和对应 cursor position；当前支持 `Ctrl+A/E/B/F/W/U/K/D`。
## Shell 模式边界

Shell 模式本身不在本模块实现；本模块只保证 `Tab` 不被误判为滚动或换行。执行逻辑在 `app.cc::submitShell()` 中。

## 边界

- 不读写 `messages_`、`input_`、`agent_bridge_`。
- 不调用 FTXUI 渲染 API。
- 不直接调用 `screen.Exit()`；退出确认由 `app.cc` 协调。
- 不维护特殊键硬编码分支；新增特殊键先更新 `keymap/` 表。
