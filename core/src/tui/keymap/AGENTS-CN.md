# core > src > tui > keymap > AGENTS-CN.md

TUI 键盘映射子模块。只保存特殊键到语义动作的静态映射表，不处理鼠标事件、不修改 UI 状态、不调用 Agent。

## 文件

| 文件 | 职责 |
|------|------|
| `keymap.hh` | `AFS_TuiKeyBindings` 大结构体数组及其枚举/结构体定义 |

## 映射边界

- `AFS_TuiKeyBindings` 是特殊键的唯一数据源：`Esc`、`Enter`、`Tab`、modified Enter、方向键、翻页键、Home/End、Backspace/Delete。
- 普通字符不进入映射表；输入组件继续按 FTXUI 默认逻辑处理，`app.cc` 只把普通字符视为会取消退出确认。
- 鼠标滚轮、侧栏按钮、目录展开、Quick index 点击和分割线拖动不是键盘映射，仍由 `app.cc` 结合坐标处理。
- 表项只描述动作意图和滚动步长；真正执行提交、退出、模式切换、滚动等副作用的是 `app.cc`。

## 可修改动作

需要改快捷键时，只修改 `AFS_TuiKeyBindings` 表项，把按键映射到已有语义动作；不要在 `app.cc` 或 `input.cc` 新增零散按键分支。

| 动作 | 当前用途 | 可安全调整项 |
|------|----------|--------------|
| `BeginExit` | 进入或确认退出 | 可改触发键；保留双击退出语义由 `app.cc` 执行 |
| `Submit` | Agent 模式提交 / Shell 模式执行 | 可改触发键；不要映射到会与多行输入混淆的 raw LF |
| `ToggleShellMode` | Agent/Shell 模式切换 | 可改触发键 |
| `InsertNewline` | 多行输入插入换行 | 可增删 modified Enter 终端序列 |
| `Scroll` | 消息区滚动 | `scroll_delta` 为期望增量，运行时由 `app.cc` 按 80% 页面高度钳制 || `CancelExitConfirmation` | 取消 `Esc` 退出确认 | 可增删特殊键；普通字符仍由 `app.cc` 处理 |

鼠标滚轮、右侧栏 `Index` / `Files` 标签、目录展开、Quick index 点击和分割线拖动不是键盘映射动作，不放入 `AFS_TuiKeyBindings`。
