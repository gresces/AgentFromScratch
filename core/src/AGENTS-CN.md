# core > src > AGENTS-CN.md

Agent 核的源代码目录，按职责分层组织。所有 C++ 代码集中于此，公共头文件位于上层的 `core/include/afs/`。

## 目录结构

| 目录 | 职责 |
|------|------|
| `basic/` | 基础设施模块 |
| ├─ `config/` | 全局配置（`AFS_Config`、`AFS_ModelConfig`）与默认用户配置目录路径解析 |
| ├─ `models/` | 模型抽象与实现（`AFS_Model`、OpenAI 兼容、DeepSeek） |
| `agent/` | Agent 核心定义与运行循环 |
| ├─ `agent.hh` / `agent.cc` | Agent 节点：树结构、工具注册、子 Agent 管理 |
| ├─ `context/` | 上下文管理器：消息历史累积、LLM 请求构建 |
| ├─ `loop/` | 核心运行循环：boost::sml 状态机，LLM ↔ 工具执行 |
| ├─ `tool/` | 工具注册与执行：`AFS_ToolRegistry`、`AFS_ToolCall` |
| `include/` | 兼容性存根目录（仅保留 AGENTS-CN.md） |
| `plugins/` | 插件系统实现 |
| ├─ `plugin_loader.hh` / `plugin_loader.cc` | 动态库加载器：RAII `AFS_LoadedPlugin` |
| ├─ `plugin_manager.hh` / `plugin_manager.cc` | 全局插件管理器：单例、引用计数、目录批量加载 |
| `tui/` | FTXUI 终端界面（`AFS_TuiApp`，Agent 前端） |
| ├─ `app.hh` / `app.cc` | TUI 应用协调层：组装布局、输入事件、Agent 交互子模块；实现 TUI-only Shell 模式 |
| ├─ `agent/` | TUI ↔ Agent/Logger/Plugin/Model 交互桥接 |
| ├─ `input/` | TUI Enter/Tab/Esc、modified Enter、键盘/鼠标滚动处理 |
| ├─ `layout/` | FTXUI 英文状态栏、消息区、输入栏布局和样式 |
| ├─ `message/` | TUI 消息数据模型（role + content + detail） |
| `main.cc` | 程序入口 |

## 文件扩展名约定

- **源文件**：`.cc`（例如 `agent.cc`、`plugin_loader.cc`）
- **头文件**：`.hh`（例如 `agent.hh`、`config.hh`）
- **头文件保护**：一律使用 `#pragma once`（不用 `#ifndef` 宏守卫）

## 各模块说明

### basic/ — 基础设施

#### basic/config/ — 全局配置

从 JSON 配置文件加载 Agent 运行所需的全部参数。核心类型：

- `AFS_ModelConfig` — 单个模型配置（name、base_url、api_key、model）
- `AFS_ModelsConfig` — 模型分组（`llms` 和 `embeddings` 两个列表）
- `AFS_Config` — 顶层配置对象，`AFS_Config::loadFromFile(path)` 返回 `std::optional<AFS_Config>`

```cpp
auto config = AFS_Config::loadFromFile("config.json");
if (!config) { /* 加载失败处理 */ }
for (const auto& llm : config->models().llms) {
    // 遍历所有 LLM 配置
}
```

#### basic/models/ — 模型抽象

模型继承体系，支持运行时多态：

- `AFS_Model`（抽象基类）— 定义 `chatCompletion()` 和 `embedding()` 虚方法
- `AFS_Model_OpenAICompatible` — 兼容 OpenAI API 协议（构造时从 `AFS_ModelConfig` 提取 base_url、api_key）
- `AFS_Model_DeepSeek` — DeepSeek 专用模型，继承自 OpenAI 兼容基类
- `createModel(cfg)` — 工厂函数，根据 `base_url` 自动识别协议类型

**约定**：模型实例不可变，所有成员数据在构造时深拷贝。通过 `std::unique_ptr<AFS_Model>` 持有。

### agent/ — Agent 核心

#### agent/ — Agent 节点（AFS_Agent）

Agent 采用**树状结构**组织。关键规则：

1. 一个程序有且只有一个主 Agent（`level == 0`）
2. 父节点通过 `unique_ptr` 独占子节点所有权
3. `genSubNode()` 返回引用（非拥有），只有父节点有权操作子节点
4. 删除一个节点时，其所有后代节点递归销毁
5. 子 Agent 继承父 Agent 的已注册工具
6. 所有 Agent 构造时自动初始化默认上下文
7. 主 Agent（`level == 0`）构造时自动调用 `registerTools()`

**Agent 内部组件**：
- `AFS_Model` — 通过 `setModel()` 注入，Agent 独占所有权。
- `AFS_Loop` — 私有成员，驱动 LLM ↔ 工具执行的完整循环。
- `AFS_Context` — 私有成员，管理对话消息历史。
Loop 仅通过 `run(context_, tool_registry_, *model_, uuid_)` 获取所需资源，运行时事件写入 `AFS_Logger` 缓冲区。

**陷阱**：子 Agent 的上下文由父级显式设置，不自动继承父级消息。

#### agent/context/ — 上下文管理器（AFS_Context）

每个 `AFS_Agent` 持有一个 `AFS_Context` 实例，负责：

- 累积对话历史（`addMessage()`、`addMessages()`）
- 构建 LLM 请求用消息数组（`buildRequest()`）
- 生成可读上下文摘要（`buildPrompt()`）

```cpp
agent.context().addMessage(AFS::UserMessage("你好"));
agent.context().addMessage(AFS::AssistantMessage("你好！有什么可以帮助你的？"));
```

#### agent/loop/ — 核心运行循环（AFS_Loop）

基于 `boost::sml` 状态机实现的 Agent 核心循环：

1. 将上下文序列化为 LLM 请求
2. 调用模型获取响应
3. 解析响应中的文本和工具调用
4. 执行工具调用，将结果写回上下文
5. 循环直到模型返回纯文本回复（无工具调用）

最大迭代次数 `kMaxIterations = 10`，防止无限循环。`ParsedResponse` 结构体分离 content、reasoning 和 tool_calls。

#### agent/tool/ — 工具注册与执行（AFS_ToolRegistry）

内部工具系统，与插件系统的关系是：插件通过 `AFS_ToolSpec` + 可调用对象注册到注册表。

- `AFS_ToolCall` — Agent 发起的单次工具调用请求（uuid、name、arguments、environment、metadata）
- `AFS_ToolResult` — 工具调用执行结果（call_uuid、success、output、error）
- `AFS_ToolRegistry` — 注册、查询、执行工具的注册表

### include/ — 兼容性存根

目录仅保留 `AGENTS-CN.md` 文档。UUID 工具函数定义已迁移至 `core/include/afs/common.hh`（`AFS::uuid8()` / `AFS::uuid16()`）。

本目录中的代码不应新增文件，新代码的全部头文件都在 `core/include/afs/` 定义。

### plugins/ — 插件系统

#### plugin_loader — 动态库加载器

`AFS_LoadedPlugin` 是 RAII 封装，持有三样东西按正确顺序释放：

1. 插件对象指针（`AFS::Plugin*`）
2. 销毁函数（`AFS::DestroyFn`）
3. 动态库句柄（`void*`，dlopen 返回）

析构顺序：先调用销毁函数销毁插件对象，再 `dlclose` 关闭动态库。**顺序不可颠倒**——先关库会导致销毁函数的代码段已卸载。

`AFS_LoadedPlugin` 只支持移动（禁止拷贝），移动后源对象置空。

#### plugin_manager — 全局插件管理器

单例（`AFS_PluginManager::instance()` 返回 `shared_ptr`），管理插件生命周期：

- **引用计数**：`loadPlugin()` 递增，`unloadPlugin()` 递减，归零时自动卸载
- **目录批量加载**：`loadFromDirectory(dir)` 预加载目录下所有插件到内存
- **文件路径约定**：`<dir>/<type>/<Type>Plugin<Name>`，例如 `${XDG_CONFIG_HOME:-~/.config}/afs/plugins/tool/ToolPluginCalculator`
- **能力查询**：`allToolCaps()` 获取所有已加载工具的 `ToolCap` 列表

**陷阱**：插件管理器只管理插件生命周期（加载/卸载），工具的注册/注销由 `AFS_Agent::registerTools()` / `AFS_Agent::removeTool()` 负责。两者解耦。

### 公共 API 实现

`AFS` 命名空间下类型的 `print()` 方法已内联到 `core/include/afs/` 对应头文件中，无需单独的 `.cc` 实现文件：

- `AFS::Message::print()` — 内联于 `core/include/afs/message.hh`
- `AFS::ToolSpec::print()` — 内联于 `core/include/afs/tool.hh`
- 公共辅助函数 `appendMeta()` — 提取到 `core/include/afs/metadata.hh`（header-only）

**约定**：内部运行时类型（`AFS_` 前缀）的 `print()` 实现仍在各自模块的 `.cc` 中（如 `agent/tool/tool.cc`），与公共 API 分离。

### main.cc — 程序入口

命令行使用：

```
./afs [config.json] [prompt]
```

执行流程：

1. 无参数时使用 `AFS_DefaultConfigPath()`（`${XDG_CONFIG_HOME:-~/.config}/afs/config.json`）并进入 TUI；显式 `config.json` 时加载该文件
2. 初始化插件管理器，从 `AFS_DefaultPluginDirectory()`（`${XDG_CONFIG_HOME:-~/.config}/afs/plugins/`）批量加载插件
3. 根据配置创建模型实例（`createModel`）
4. 创建主 Agent（`AFS_Agent::createMain`），自动注册所有工具
5. 将命令行 prompt 作为用户消息加入上下文
6. 运行 `AFS_Loop::run()` 进入 Agent 循环
7. 按消息顺序输出思考过程、工具调用和结果；控制台模式会把 ToolResult JSON 字符串字段解码后显示，`\n` / `\t` 等转义呈现为真实换行/制表
8. 输出最终回复

## 依赖关系

```
main.cc
 ├─ basic/config/     (配置加载)
 ├─ basic/models/     (模型创建)
 ├─ plugins/          (插件管理器，加载 .so)
 └─ agent/
      ├─ agent/       (Agent 节点)
      ├─ context/     (上下文)
      ├─ loop/        (运行循环 → 调用 model)
      └─ tool/        (工具注册表)
```

## 注意事项

- **头文件依赖**：`core/src/**/*.cc` 引用 `core/include/afs/` 下的公共头文件（如 `<afs/message.hh>`）。`core/src/` 内部不定义公共类型。
- **插件 .so 路径**：默认插件目录为 `${XDG_CONFIG_HOME:-~/.config}/afs/plugins/`，`plugin_manager` 按 `<type>/` 子目录查找。
- **单例生命周期**：`AFS_PluginManager` 是 `shared_ptr` 单例，在 `main()` 结束前不会析构——不要在全局析构阶段访问它。
- **状态机**：`AFS_Loop` 基于 `boost::sml`，修改循环逻辑前需了解 sml 的事件/状态/转换模型。
