# core > src > tui > AGENTS-CN.md

基于 FTXUI 的终端用户界面，作为 Agent 核心的前端模块。不是独立程序入口；`main.cc` 在无命令行 prompt 时调用 TUI，有 prompt 时走控制台模式。

## 文件与子模块

| 路径 | 职责 |
|------|------|
| `app.hh` / `app.cc` | TUI 应用协调层：组装 FTXUI 组件、管理 UI 状态、调度输入/布局/Agent 交互子模块；也负责 TUI-only shell 执行线程 |
| `agent/` | Agent 核心交互层：加载配置/插件/模型，提交用户消息，后台运行 `AFS_Agent::run()`，从 Logger 轮询事件 |
| `input/` | 输入事件处理：通过 `keymap/` 的语义动作表解析 Enter/modified Enter/Tab/Esc/滚轮/键盘滚动，不渲染业务内容 |
| `keymap/` | 键盘映射表：集中维护特殊键到 TUI 语义动作的静态数组 |
| `layout/` | FTXUI 布局与样式：状态栏、消息区、右侧栏、输入栏、Input 样式 |
| `message/` | TUI 消息数据模型：`TuiMessage` 的 role + content + detail |

每个子目录必须有自己的 `AGENTS-CN.md`，说明该子模块边界和约定。

## 配置模式

配置模式提供配置文件编辑界面，通过 `Ctrl+P` 激活，`Ctrl+P` 或 `Esc` 返回聊天界面。

### 激活与退出

- `Ctrl+P`：刷新并进入配置模式；此时 Shell 模式退出，文件候选清空
- `Ctrl+P`（配置模式下）：返回聊天界面
- `Esc`：第一次显示退出确认提示，再按一次返回聊天界面

### 导航

| 快捷键 | 行为 |
|--------|------|
| `←` / `→` | 切换已注册配置分类（如 `models.llms`、`models.embeddings`、`tui.layout`、插件 schema） |
| `↑` / `↓` | 切换当前分类下的可编辑配置项 |

### 保存

- `Enter` / `Ctrl+S`：解析当前选中配置项输入框的内容，通过 `AFS_ConfigManager::updateValue()` 写回 JSON，保存到磁盘，并按受影响模块决定是否重载模型配置
- 保存后状态栏显示操作结果（成功 / 失败原因）；`tui.layout.sidebar_ratio` 保存时会按运行时范围钳制

### 布局

```
 Config ───────────────────── Enter/Ctrl+S save, arrows select, Esc return
──────────────────────────────────────────────────────────────────────────────
 models.llms      │ Path: models.llms.0.name                                  │
  [0] DeepSeek / name                                                          │
  [0] DeepSeek / base_url                                                      │
  [0] DeepSeek / api_key                                                       │
  [0] DeepSeek / model                                                         │
  [0] DeepSeek / context_limit                                                 │
 models.embeddings                                                             │
 tui.layout                                                                    │
  sidebar_ratio                                                                │
──────────────────────────────────────────────────────────────────────────────
 Config loaded: /home/user/.config/afs/config.json
```

- 左侧：配置分类与配置项列表；右侧：选中项的路径、类型、当前值和可编辑输入框
- 当前选中分类和项目高亮（蓝底白字）；敏感字段在详情中以 `***` 显示，但输入框用于编辑真实配置值
- 底部状态行显示加载/保存结果

### 实现

- `AFS_TuiApp::registerConfigSchema()`：注册 TUI 自有 `tui.layout` schema；模型和插件 schema 由各自模块注册
- `AFS_TuiApp::refreshConfigView()`：读取配置文件 → 遍历 `AFS_ConfigManager::schemas()` 构建分类和配置项 → 同步当前项输入框
- `AFS_TuiApp::saveAndReloadConfig()`：解析当前输入框 → 写回选中 JSON 路径 → 保存到磁盘 → 按受影响模块应用配置；模型配置变更会通知 `agent_bridge_->reloadConfig()`
- `AFS_TuiApp::moveConfigSelection(dc, di)`：按 delta 移动分类/项目索引，自动 clamp 到有效范围，并同步输入框
- 渲染由 `AFS_TuiRenderConfigMode(const AFS_TuiConfigView&, Component)` 完成，位于 `layout/layout.cc`
  渲染时若 `config_mode_` 为 true，主渲染 lambda 直接返回配置视图，跳过消息区和普通输入栏

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
    void refreshConfigView();       // 加载配置文件到动态配置视图
    void saveAndReloadConfig();     // 保存当前配置项并按受影响模块重载
    void moveConfigSelection(int category_delta, int item_delta);

    std::unique_ptr<AFS_TuiAgentBridge> agent_bridge_;
    std::vector<TuiMessage> messages_;
    std::vector<AFS_TuiQuickIndexEntry> quick_index_entries_;
    std::vector<AFS_TuiFileEntry> file_entries_;
    std::set<std::filesystem::path> expanded_file_dirs_;
    AFS_TuiSidebarButtons sidebar_buttons_;
    std::string input_;
    std::filesystem::path config_path_;
    int scroll_position_ = 1000;
    int spinner_frame_ = 0;
    std::vector<AFS_TuiConfigCategory> config_categories_;  // 配置浏览器分类
    int config_category_index_ = 0;
    int config_item_index_ = 0;
    std::string config_status_;
    bool config_mode_ = false;
    bool esc_pending_ = false;
    double sidebar_ratio_ = 0.35;
    bool resizing_sidebar_ = false;
    bool shell_mode_ = false;
    AFS_TuiSidebarMode sidebar_mode_ = AFS_TuiSidebarMode::QuickIndex;
    bool follow_latest_ = true;
    ftxui::Box sidebar_splitter_box_;
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
- 键盘语义映射：`keymap/keymap.hh`
- 消息模型：`message/message.hh`
- TUI-only shell 命令执行：`app.cc::submitShell()`，不进入 Agent 上下文
- 配置模式查看与保存：`app.cc::refreshConfigView()` / `saveAndReloadConfig()`，不进入 Agent 上下文

## 界面文字

TUI 运行时默认显示英文文本。文档可继续使用中文，代码中的用户可见 TUI 文案应保持英文。

## 界面布局

```
窄屏（<= 130 列）
⠋ Agent running ...          AGENT | deepseek-v4-pro | main/bin | ctx 5
-- user ---------------------------------------------------------------------
echo hello
...
──────────────────────────────────────────────────────────────────────────────
 > user input

宽屏（> 130 列）
⠋ Agent running ...          AGENT | deepseek-v4-pro | main/bin | ctx 5
-- user ---------------------------------------------- │ Index  Files
echo hello                                             │ Quick index
-- assistant ----------------------------------------- │ 1. first prompt
...                                                    │ 2. second prompt
                                                       │
                                                       │ Directory view
──────────────────────────────────────────────────────────────────────────────
 > user input
```

约定：
- 状态栏只占一行，元素之间用 `|` 连接。状态栏与消息区间无分隔线。
- 不显示外侧边框。
- 消息区不设固定最大宽度；宽度跟随终端和右侧栏比例变化，右侧栏拖动后按新的左侧可用区域重新折行。
- 终端宽度超过 130 列时启用右侧栏；右侧栏顶部有 `Index` / `Files` 标签按钮，不显示边框或动画，当前按钮使用下划线标识。
- `Index` 显示 Quick index；`Files` 显示当前工作目录的文件树，点击可展开目录，再次点击折叠。
- 右侧区域宽度比例保存到配置文件 `tui.layout.sidebar_ratio`，下次启动 TUI 时恢复。
- Quick index 只列出用户输入；鼠标点击条目会把消息区滚动到对应用户消息附近。
- Agent 模式下输入以 `@` 开头时，输入栏下方显示最多 6 个文件路径候选；`↑` / `↓` 移动高亮，`Tab` 将当前高亮候选写入输入栏且不切换 Shell 模式。
- Agent 模式下输入单个 `.` 并提交时，实际发送 `keep going`。
- 输入栏通过独立 `cursor_position` 引用交给 FTXUI `Input` 组件渲染真实光标位置；聚焦时文字加粗，未聚焦时文本灰化（`dim`），不显示反转背景或下划线。
- Tool 消息正文不显示 `[tool call_id=...]` 前缀；`call_id=...` 放入 `-- tool call_id=... ----` header。
- Header 中只有 role 名称使用角色颜色和加粗；`call_id=...` 等 detail 使用 dim，不突出显示。

## FTXUI 组件树

```
main_component (Renderer + CatchEvent)
  └── Renderer(input_component, lambda)
        ├── AFS_TuiRenderStatus(...)
        ├── content_area
        │     ├── message_area
        │     │     ├── AFS_TuiRenderMessages(messages_)
        │     │     ├── focusPositionRelative(0.0F, scroll_position)
        │     │     ├── frame + vscroll_indicator
        │     │     └── size(WIDTH, EQUAL, message_region_width)
        │     └── width > 130 时追加 draggable splitter + AFS_TuiRenderSidebar(...)
        ├── separator()
        └── AFS_TuiRenderInput(input_component, shell_mode_, file_candidates)
```

- `input_component` 是唯一交互组件。
- 消息区和状态栏是纯 `Element`，不参与焦点导航。
- `CatchEvent` 挂在最外层，优先消费全局快捷键。
- Quick index 行使用 `reflect(Box&)` 记录点击区域，点击事件由 `app.cc` 根据鼠标坐标映射到 `scroll_position_`。
- 中间分割线使用 `reflect(Box&)` 记录点击区域；鼠标拖动时更新 `sidebar_ratio_`，释放左键时写回配置文件。
- 右侧栏按钮使用 `reflect(Box&)` 记录点击区域；`app.cc` 根据坐标切换 `QuickIndex` / `Files`。
- 文件目录行使用 `reflect(Box&)` 记录点击区域；`app.cc` 点击可展开目录时更新 `expanded_file_dirs_` 并重建可见文件树。
- `screen.TrackMouse(true)` 启用鼠标滚轮事件。

## 模式与快捷键

| 快捷键 | 行为 |
|--------|------|
| `Enter` | Agent 模式：提交消息进入上下文并运行 Agent；Shell 模式：执行当前输入的 bash 命令 |
| `Tab` | 默认在 Agent 模式和 Shell 模式之间切换；`@` 文件候选激活时补全当前高亮候选 |
| `Ctrl+Enter` | 插入换行；识别支持 modified Enter 的终端序列 |
| `Shift+Enter` | 插入换行；识别支持 modified Enter 的终端序列 |
| `Esc` | 第一次进入退出确认，状态栏提示 `Press Esc again to exit` |
| `Esc` 再按一次 | `screen.Exit()` 退出程序 |
| 任意普通按键 | 取消退出确认 |
| `↑` / `↓` | 文件候选激活时移动高亮；否则消息区逐行滚动（增量 = 页面高度 / 6，根据终端高度和消息数量动态计算） |
| `PageUp` / `PageDown` | 消息区翻页滚动（增量 = 80% 页面高度，根据终端高度和消息数量动态计算） |
| 鼠标滚轮 | 消息区滚动（步长 = 页面高度 / 6，根据终端高度和消息数量动态计算） |
| `Ctrl+A` / `Ctrl+E` | 移动输入光标到当前行首 / 行尾 |
| `Ctrl+B` / `Ctrl+F` | 输入光标左移 / 右移一个字符 |
| `Ctrl+W` | 删除输入光标前一个单词 |
| `Ctrl+U` / `Ctrl+K` | 删除当前行中光标前 / 光标后的内容 |
| `Ctrl+D` | 删除输入光标处的字符 |
| `Ctrl+S` | 配置模式下保存并重载配置 |


不要把 `Ctrl+J` 当作多行输入快捷键。FTXUI 中 plain Enter 和 `Ctrl+J` 都是 LF；TUI 只把 plain LF 作为提交处理。多行输入仅匹配 modified Enter 序列（如 `ESC [ 13 ; 2 u` / `ESC [ 13 ; 5 u`）。

特殊键到动作事件的映射集中在 `keymap/keymap.hh` 的 `AFS_TuiKeyBindings` 大结构体数组中；`app.cc` 只消费解析后的 `AFS_TuiKeyAction`。

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
        │     ├── AgentEvent → TuiMessage（delta 事件合并到上一条同角色消息）
        │     └── 若 `follow_latest_` 为 true → 自动跟滚到底部
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

新消息到达时，若 `follow_latest_` 为 true，渲染时通过 `focusPositionRelative(0.0F, 1.0F)` 跟随到底部。用户手动上滚会关闭跟滚；继续向下滚到底部会重新开启跟滚。`scroll_position_` 是 0..1000 的相对位置。

按 `Esc` → 状态栏显示黄色 `Press Esc again to exit` → 再按 `Esc` → `screen.Exit()` → 停止轮询线程并返回。任意普通按键取消退出确认。

## 依赖

- `ftxui`：终端 UI 框架
- `core/src/basic/log/logger.hh`：事件缓冲和轮询
- `core/src/basic/config/config.hh`：动态配置管理器和 schema 注册类型
- `core/src/basic/models/model.hh`：模型配置结构和模型创建
- `core/src/plugins/plugin_manager.hh`：插件加载

## 约定

- `AFS_TuiApp` 不继承插件接口，是纯应用层代码。
- TUI 不直接访问 Loop 内部；Agent 运行状态和消息事件经 `AFS_TuiAgentBridge` / `AFS_Logger::poll()` 传递。
- FTXUI 滚动基于 `frame` + `focusPosition` + `vscroll_indicator`。
- 输入栏 `multiline = true` 只用于显示和保存换行；提交由外层 `CatchEvent` 接管。
- 输入栏和配置编辑框分别持有独立 cursor position，并通过 `AFS_TuiInputOption(&cursor)` 传入 FTXUI `InputOption::cursor_position`；不要用外层 cursor decorator 伪造光标，否则多行输入时位置会偏移。
- 滚动增量由 `app.cc` 根据终端高度和消息数量动态计算（`page_units = 1000 * frame_lines / content_lines`）。PageUp/PageDown 使用 `±(page_units * 0.8)`，ArrowUp/ArrowDown 和鼠标滚轮使用 `±(page_units / 6)`。`input.cc` 仅做 0..1000 范围裁剪。

### 配置模式组件树

```
config_mode_ == true 时：
main_component
  └── Renderer(…)
        ├── AFS_TuiRenderStatus(…)
        └── AFS_TuiRenderConfigMode(config_view, config_edit_component)
              ├── header: "Config" + 分类列表 + 提示文字
              ├── hbox: configItems(view) | configDetail(view, edit input)
              └── status: 操作结果文字
```
