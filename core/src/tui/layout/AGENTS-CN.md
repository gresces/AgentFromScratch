# core > src > tui > layout > AGENTS-CN.md

TUI 布局与样式子模块。负责把 UI 状态渲染为 FTXUI `Element` / `InputOption`，不处理事件，不访问 Agent。

## 文件

| 文件 | 职责 |
|------|------|
| `layout.hh` | 布局渲染 API 与 `AFS_TuiStatusView`、右侧栏数据结构 |
| `layout.cc` | 状态栏、消息区、右侧栏、输入栏样式实现 |

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

struct AFS_TuiQuickIndexEntry {
    std::string label;
    int scroll_position;
    ftxui::Box box;
};

enum class AFS_TuiSidebarMode { QuickIndex, Files };

struct AFS_TuiSidebarButton {
    AFS_TuiSidebarMode mode;
    std::string label;
    ftxui::Box box;
};

struct AFS_TuiFileEntry {
    std::string label;
    std::filesystem::path path;
    int depth;
    bool is_directory;
    bool is_expanded;
    bool is_expandable;
    ftxui::Box box;
};

struct AFS_TuiFileCandidates {
    bool active;
    int selected_index;
    int visible_offset;
    std::vector<std::string> labels;
};

ftxui::InputOption AFS_TuiInputOption();
ftxui::Element AFS_TuiRenderStatus(const AFS_TuiStatusView& view);
ftxui::Element AFS_TuiRenderMessages(const std::vector<TuiMessage>& messages);
ftxui::Element AFS_TuiRenderQuickIndex(std::vector<AFS_TuiQuickIndexEntry>& entries);
ftxui::Element AFS_TuiRenderSidebar(AFS_TuiSidebarMode mode, AFS_TuiSidebarButtons& buttons,
                                    std::vector<AFS_TuiQuickIndexEntry>& quick_index_entries,
                                    std::vector<AFS_TuiFileEntry>& file_entries);
ftxui::Element AFS_TuiRenderInput(ftxui::Component input_component, bool shell_mode,
                                  const AFS_TuiFileCandidates& file_candidates);
```

## 用户可见文案

TUI 默认使用英文文案：`Ready`、`Agent running ...`、`Shell ready`、`Shell running ...`、`Press Esc again to exit`、`Type a message to start ...`。

## 样式约定

状态栏：单行显示。左侧显示 ready/running + spinner；右侧显示模式（`AGENT` / `SHELL`）、模型名、工作目录、上下文条数。元素之间用 `|` 连接。
- 消息区宽度由 `app.cc` 按当前可用区域传入；右侧栏拖动后重新渲染并按新宽度折行，不设置 120 列上限。
- 终端宽度超过 130 列时，`app.cc` 在消息区右侧追加可拖动分割线和右侧栏；分割线拖动会更新布局比例并改变消息区可用宽度。
- 右侧栏顶部渲染 `Index` / `Files` 标签按钮，不显示边框或动画；当前按钮使用下划线标识，按钮通过 `reflect(button.box)` 记录坐标，本模块不处理点击。
- Quick index 每一行通过 `reflect(entry.box)` 记录实际坐标；本模块只渲染和记录 box，不处理点击跳转或分割线拖动。
- `esc_pending == true` 时状态栏只显示黄色 `Press Esc again to exit`。
- 消息区按 `TuiMessage::Role` 着色：
  - `User`：绿色
  - `Assistant`：青色
  - `Thinking`：深灰，正文整体 `dim`
  - `Tool`：黄色
  - `Shell`：品红
- 消息 role header 使用 `-- role detail ----` 形式；`detail` 为空时省略。
- 只有 role 名称使用角色颜色和加粗；`detail`（例如 `call_id=...`）使用 `dim`，不要突出显示。
- Tool 消息的 `call_id=...` 放在 header 的 `detail` 中，正文只显示 JSON 或输出内容。
- Assistant 正文不显示 `[assistant]` 前缀。
- 内容使用 `paragraph()` 自动换行。
- 输入栏通过 `InputOption::transform` 去除反转背景，只使用 `dim`；不要加 `underlined`。
- 输入提示符：Agent 模式为 `>`，Shell 模式为 `$`。
- 输入栏下方可显示 `@` 文件候选列表；候选区最多显示 6 项，通过 `selected_index` 高亮当前项，本模块不处理选择或改写输入。
- Quick index 标题使用英文 `Quick index`；无用户消息时显示 `No user messages`。
- 文件夹目录标题使用英文 `Directory`；可展开目录显示 `[+] name` / `[-] name`，不可展开目录显示 `[D] name`，文件显示 `[F] name`，子项按 `depth` 缩进。
- 右侧栏宽度由 `tui.layout.sidebar_ratio` 间接决定；本模块不读取配置文件。

## 边界

- 不修改 `scroll_position_` / `follow_latest_` / `sidebar_mode_` / `expanded_file_dirs_`；滚动、模式切换和目录展开由 `input/` 和 `app.cc` 协调。
- 不调用 `AFS_Logger` / `AFS_Agent`。
- 不创建线程。
