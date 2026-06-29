# core > src > tui > AGENTS-CN.md

基于 FTXUI 的终端用户界面，作为 Agent 核心的前端模块。不是独立程序入口；`main.cc` 在无命令行 prompt 时调用 TUI，有 prompt 时走控制台模式。

## 文件与子模块

| 路径 | 职责 |
|------|------|
| `app.hh` / `app.cc` | TUI 应用协调层：组装 FTXUI 组件、管理 UI 状态、调度输入/布局/Agent 交互子模块；也负责 TUI-only shell 执行线程 |
| `agent/` | Agent 核心交互层：加载配置/插件/模型，提交用户消息，后台运行 `AFS_Agent::run()`，从 Logger 轮询事件 |
| `input/` | 输入事件处理：Enter/modified Enter/Tab/Esc/滚轮/键盘滚动，不渲染业务内容 |
| `layout/` | FTXUI 布局与样式：状态栏、消息区、输入栏、Input 样式 |
| `message/` | TUI 消息数据模型：`TuiMessage` 的 role + content + detail |

每个子目录必须有自己的 `AGENTS-CN.md`，说明该子模块边界和约定。

## 入口选择

```
main.cc
  ├── argc == 1  (无参数) → AFS_TuiApp::create(AFS_DefaultConfigPath()) → run()
  ├── argc == 2  (config.json) → AFS_TuiApp::create(config) → run()
  └── argc >= 3  (config + prompt) → 控制台模式
```

TUI 模式下不接收命令行 prompt；用户通过 TUI 输入栏交互。无参数启动时读取 `${XDG_CONFIG_HOME:-~/.config}/afs/config.json`，插件默认从 `${XDG_CONFIG_HOME:-~/.config}/afs/plugins/` 加载。

## `AFS_TuiApp`

```cpp
class AFS_TuiApp {
  public:
    static std::unique_ptr<AFS_TuiApp> create(const std::string& config_path);
    void run();

  private:
    void submit();       // Agent 模式：写入上下文并运行 Agent
    void submitShell();  // Shell 模式：执行 /bin/bash -lc，不写入上下文
    void pollEvents();

    std::unique_ptr<AFS_TuiAgentBridge> agent_bridge_;
    std::vector<TuiMessage> messages_;
    std::string input_;
    int scroll_offset_ = 0;
    int spinner_frame_ = 0;
    bool esc_pending_ = false;
    bool shell_mode_ = false;
    std::atomic<bool> shell_running_{false};
    std::atomic<bool> tui_running_{false};
    std::thread shell_thread_;
    std::mutex messages_mutex_;
};
```

`AFS_TuiApp` 只做协调：
- Agent 生命周期和 Logger 事件转消息：`agent/bridge.*`
- 输入/快捷键/滚动判断：`input/input.*`
- FTXUI 元素和样式：`layout/layout.*`
- 消息模型：`message/message.hh`
- TUI-only shell 命令执行：`app.cc::submitShell()`，不进入 Agent 上下文

## 界面文字

TUI 运行时默认显示英文文本。文档可继续使用中文，代码中的用户可见 TUI 文案应保持英文。

## 界面布局

```
⠋ Agent running ...          AGENT | deepseek-v4-pro | main/bin | ctx 5
-- user ---------------------------------------------------------------------
echo hello

-- assistant ----------------------------------------------------------------
I will run the command ...

-- tool call_id=call_xxx ----------------------------------------------------
{"output":"hello\n","exit_code":0}

-- shell command ------------------------------------------------------------
$ pwd

-- shell exit_code=0 --------------------------------------------------------
/home/user/project/bin
──────────────────────────────────────────────────────────────────────────────
 > user input
```

约定：
- 状态栏只占一行，元素之间用 `|` 连接。状态栏与消息区间无分隔线。
- 不显示外侧边框。
- 输入栏不使用反转背景，不显示下划线。
- Assistant 消息正文不显示 `[assistant]` 前缀；role 只出现在 `-- assistant ----` header。
- Tool 消息正文不显示 `[tool call_id=...]` 前缀；`call_id=...` 放入 `-- tool call_id=... ----` header。
- Header 中只有 role 名称使用角色颜色和加粗；`call_id=...` 等 detail 使用 dim，不突出显示。

## FTXUI 组件树

```
main_component (Renderer + CatchEvent)
  └── Renderer(input_component, lambda)
        ├── AFS_TuiRenderStatus(...)
        ├── AFS_TuiRenderMessages(messages_)
        │     | focusPosition(0, scroll_offset_)
        │     | frame
        │     | vscroll_indicator
        │     | flex
        ├── separator()
        └── AFS_TuiRenderInput(input_component, shell_mode_)
```

- `input_component` 是唯一交互组件。
- 消息区和状态栏是纯 `Element`，不参与焦点导航。
- `CatchEvent` 挂在最外层，优先消费全局快捷键。
- `screen.TrackMouse(true)` 启用鼠标滚轮事件。

## 模式与快捷键

| 快捷键 | 行为 |
|--------|------|
| `Enter` | Agent 模式：提交消息进入上下文并运行 Agent；Shell 模式：执行当前输入的 bash 命令 |
| `Tab` | 在 Agent 模式和 Shell 模式之间切换；Shell 模式只在 TUI 中可用 |
| `Ctrl+Enter` | 插入换行；识别支持 modified Enter 的终端序列 |
| `Shift+Enter` | 插入换行；识别支持 modified Enter 的终端序列 |
| `Esc` | 第一次进入退出确认，状态栏提示 `Press Esc again to exit` |
| `Esc` 再按一次 | `screen.Exit()` 退出程序 |
| 任意普通按键 | 取消退出确认 |
| `↑` / `↓` | 消息区上/下滚动 |
| `PageUp` / `PageDown` | 消息区翻页滚动 |
| 鼠标滚轮 | 消息区滚动 |

不要把 `Ctrl+J` 当作多行输入快捷键。FTXUI 中 plain Enter 和 `Ctrl+J` 都是 LF；TUI 只把 plain LF 作为提交处理。多行输入仅匹配 modified Enter 序列（如 `ESC [ 13 ; 2 u` / `ESC [ 13 ; 5 u`）。

## Agent 消息流

```
用户在 Agent 模式按 Enter
  │
  ├── AFS_TuiApp::submit()
  │     ├── 追加 TuiMessage{User, input}
  │     └── AFS_TuiAgentBridge::submitUserMessage()
  │           ├── context.addMessage(UserMessage)
  │           └── 后台线程: agent->run()
  │
  └── 轮询线程 (100ms)
        ├── AFS_TuiApp::pollEvents()
        │     ├── AFS_TuiAgentBridge::pollMessages()
        │     ├── AFS_Logger::poll() → vector<AgentEvent>
        │     ├── AgentEvent → TuiMessage
        │     └── 若在底部 → 自动跟滚
        └── screen.Post(Event::Custom)
```

## Shell 模式消息流

```
用户按 Tab 进入 Shell 模式
  │
  └── 按 Enter
        ├── AFS_TuiApp::submitShell()
        ├── 追加 TuiMessage{Shell, "$ <command>", "command"}
        ├── 后台线程执行 /bin/bash -lc '<command>' 2>&1
        └── 追加 TuiMessage{Shell, output 或 "(no output)", "exit_code=N"}
```

Shell 模式约束：
- 仅 TUI 可用；控制台模式仍由 Agent 工具系统处理 shell/bash 调用。
- 不调用 `AFS_TuiAgentBridge::submitUserMessage()`。
- 不写入 `AFS_Context`，不会影响上下文条数和后续 LLM 请求。
- 在进程当前工作目录执行；通过 `xmake run Agent ...` 启动时通常是 `bin/`。
- 同一时间只允许一个 shell 命令运行。

## 自动跟滚

新消息到达时，若 `scroll_offset_` 已在底部（`>= messages_.size() - 1`）或消息区为空，自动将 `scroll_offset_` 更新到最新位置。用户手动上滚后不再跟滚。

## 退出

按 `Esc` → 状态栏显示黄色 `Press Esc again to exit` → 再按 `Esc` → `screen.Exit()` → 停止轮询线程并返回。任意普通按键取消退出确认。

## 依赖

- `ftxui`：终端 UI 框架
- `core/src/basic/log/logger.hh`：事件缓冲和轮询
- `core/src/agent/agent.hh`：Agent 核心
- `core/src/basic/config/config.hh` / `core/src/basic/models/model.hh`：模型配置和创建
- `core/src/plugins/plugin_manager.hh`：插件加载

## 约定

- `AFS_TuiApp` 不继承插件接口，是纯应用层代码。
- TUI 不直接访问 Loop 内部；Agent 运行状态和消息事件经 `AFS_TuiAgentBridge` / `AFS_Logger::poll()` 传递。
- FTXUI 滚动基于 `frame` + `focusPosition` + `vscroll_indicator`。
- 输入栏 `multiline = true` 只用于显示和保存换行；提交由外层 `CatchEvent` 接管。
- 输入栏通过 `InputOption::transform` 去除反转背景，不使用 underline。
- 新增 TUI 功能优先放入现有子模块；不要把布局、输入事件、Agent 交互逻辑塞回单个大函数。
