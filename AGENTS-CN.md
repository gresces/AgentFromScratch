# AGENTS-CN.md

这是一个使用 C++ 编写的个人 `Coding Agent` 项目，目标是构建一个高性能、可拓展、可在终端独立运行的 Agent 核。

## 项目目标

- 先完成 Agent 核：负责对话循环、任务分解、工具调用、上下文管理、状态记录和错误恢复等核心能力。
- Agent 核应首先作为一个二进制程序在终端运行，保证最小可用闭环清晰、稳定、可测试。
- 展示层在 Agent 核稳定后构建，用于提供更好的视觉效果和交互体验，但不应反向污染核心逻辑。

## 技术栈

- **语言**: C++23
- **构建系统**: Xmake v3，入口 `core/xmake.lua`
- **命名空间**: `AFS`（Agent From Scratch），源代码内部类型使用 `AFS_` 前缀
- **输出目录**: 编译产物统一输出到仓库顶层 `bin/`

### 依赖

| 库 | 用途 |
|---|---|
| `nlohmann_json` | JSON 构造与解析 |
| `ftxui` | 终端 UI 框架（展示层预留） |
| `boost_sml` | Agent 核心状态机（状态转移） |
| `taskflow` | 状态内部任务并行 |
| `cpr` | HTTP 网络请求（libcurl 封装） |

## 快速开始

### 构建

```sh
cd core
xmake
```

编译产物输出到 `bin/afs`。LSP 辅助文件 `compile_commands.json` 自动生成在 `core/` 目录。

### 运行

```sh
./bin/afs                              # TUI 模式，读取 ${XDG_CONFIG_HOME:-~/.config}/afs/config.json
./bin/afs <config.json>                # TUI 模式，显式配置文件
./bin/afs <config.json> <prompt>       # 控制台模式，显式配置文件 + 单次问答
```

TUI 模式默认英文界面；输入栏按 `Tab` 切换 Shell 模式，Shell 命令在当前工作目录通过 `/bin/bash -lc` 执行，结果只显示在 TUI 中，不加入 Agent 上下文。
插件默认从 `${XDG_CONFIG_HOME:-~/.config}/afs/plugins/` 加载；配置文件默认是 `${XDG_CONFIG_HOME:-~/.config}/afs/config.json`。

### 安装

```sh
cd core

# 确保没有 afs 进程正在运行（否则安装会报 "file busy"）
pkill afs || true

sudo xmake install --root
```

`sudo xmake install --root` 安装：
- 可执行文件：`/usr/local/bin/afs`；
- 插件开发公共头文件：`/usr/local/include/afs.hh` 与 `/usr/local/include/afs/*.hh`。

如需安装到其他 prefix：

```sh
sudo xmake install --root -o /opt/afs
```

控制台示例：

```sh
./bin/afs test_config.json "What is 2+2?"
```

### 配置文件格式

```json
{
  "models": {
    "llms": [
      {
        "name": "DeepSeek",
        "base_url": "https://api.deepseek.com",
        "api_key": "sk-xxxx",
        "model": "deepseek-v4-pro"
      }
    ]
  }
}
```

### 编译插件

```sh
cd plugins
./build.sh              # 编译全部
./build.sh install      # 安装到 ${XDG_CONFIG_HOME:-~/.config}/afs/plugins/
```

## 目录地图

```
AgentFromScratch/
├── AGENTS-CN.md              ← 本文件：项目顶层描述
├── DEV-STATUS.md             ← 开发状态记录（阶段、已完成、阻塞点）
├── TODO.md                   ← 顶层待办（两级：任务 → 子任务）
├── README.md                 ← 项目简介
├── .gitignore
│
├── core/                     ← Agent 核（主程序）
│   ├── AGENTS-CN.md          ← 核级描述
│   ├── .gitignore
│   ├── xmake.lua             ← Xmake 构建定义
│   ├── compile_commands.json ← LSP 辅助（自动生成）
│   │
│   ├── include/              ← **公共 API 头文件（插件开发者入口）**
│   │   ├── AGENTS-CN.md      ← 公共 API 文档（类型定义、插件基类、示例）
│   │   ├── afs.hh            ← 总入口头文件
│   │   └── afs/
│   │       ├── common.hh     ← UUID 工具（uuid8 / uuid16）
│   │       ├── message.hh    ← 消息类型（Role、Message、便捷子类）
│   │       ├── metadata.hh   ← 公共 metadata 辅助（appendMeta）
│   │       ├── plugin.hh     ← 插件接口（Plugin、PluginType、导出宏）
│   │       └── tool.hh       ← 公共 ToolSpec 类型
│   │
│   └── src/                  ← 源代码
│       ├── AGENTS-CN.md      ← src 级描述（分层约定）
│       ├── main.cc           ← 程序入口
│       ├── include/          ← 内部兼容包装（已迁移至 core/include）
│       │
│       ├── basic/            ← 基础设施模块
│       │   ├── AGENTS-CN.md
│       │   ├── config/       ← AFS_Config 配置模块
│       │   │   └── AGENTS-CN.md
│       │   └── models/       ← 模型抽象层
│       │       └── AGENTS-CN.md
│       │
│       ├── agent/            ← Agent 核心定义
│       │   ├── AGENTS-CN.md  ← AFS_Agent 类、树状结构、所有权模型
│       │   ├── agent.hh / .cc
│       │   ├── loop/         ← 对话循环（boost::sml 状态机）
│       │   │   └── AGENTS-CN.md
│       │   ├── context/      ← 上下文管理器（消息历史）
│       │   │   └── AGENTS-CN.md
│       │   └── tool/         ← 工具调用模块（注册、执行）
│       │       └── AGENTS-CN.md
│       │
│       └── plugins/          ← 插件系统实现
│           └── AGENTS-CN.md  ← PluginManager、PluginLoader、启动流程
│       └── tui/              ← FTXUI 终端界面（Agent 前端）
│           ├── AGENTS-CN.md
│           ├── agent/        ← Agent 交互桥接
│           ├── input/        ← Enter/Tab/Esc、modified Enter、滚动
│           ├── layout/       ← 英文状态栏、消息区、输入栏样式
│           └── message/      ← TUI 消息模型（role/content/detail）
│
├── plugins/                  ← 插件源码（独立于 core 编译）
│   ├── AGENTS-CN.md          ← 插件目录说明、编译方法、命名规则
│   ├── build.sh              ← 顶层编译脚本（自动发现子插件）
│   └── tools/                ← 工具插件源码
│       ├── bash/             ← bash 命令执行工具插件
│       │   └── AGENTS-CN.md
│       └── compute/          ← 示例：compute 工具插件
│           └── AGENTS-CN.md
│
└── bin/                      ← 编译产物
    └── afs                   ← 开发构建产物；xmake install 安装到 /usr/local/bin/afs

${XDG_CONFIG_HOME:-~/.config}/afs/
    ├── config.json           ← 默认配置文件
    └── plugins/              ← 默认插件目录
        ├── tool/             ← 工具类插件（命名: ToolPlugin<Name>）
        └── skill/            ← 技能类插件（命名: SkillPlugin<Name>）
```

## 关键约定

### 文件约定

- **源文件**: `.cc` 扩展名
- **头文件**: `.hh` 扩展名，以 `#pragma once` 开头
- **文件名**: `snake_case.cc` / `snake_case.hh`
- 一个文件围绕一个模块组织，避免混合无关逻辑

### 命名约定

| 类别 | 风格 | 示例 |
|------|------|------|
| 类 / 结构体 | `PascalCase` | `AFS_Agent`、`AFS_PluginManager` |
| 函数 / 方法 | `camelCase` | `genSubNode()`、`buildRequest()` |
| 局部变量 / 字段 | `snake_case` | `tool_registry_`、`api_key_` |
| 私有成员 | `snake_case` + 尾下划线 | `level_`、`uuid_` |
| 公开常量 | `PascalCase` | `PluginAbiVersion` |
| 内部类型（非 AFS 命名空间） | `AFS_` 前缀 + PascalCase | `AFS_Config`、`AFS_ToolRegistry` |
| 公共 API 类型（AFS 命名空间） | `AFS::` 前缀 + PascalCase | `AFS::Message`、`AFS::Plugin` |

### 代码风格

- 4 空格缩进，无 Tab
- 左花括号与声明同行（Attach 风格）
- 单行 ≤ 100 字符
- 指针/引用贴近类型：`std::string&`、`AFS_Agent*`
- 早返回处理错误和边界条件
- `public` 在前，`private` 在后；访问控制标签不缩进
- 类内方法按职责分组，每组前加 `// ---- 组名 ----` 分隔注释

### 包含顺序

1. 当前 `.cc` 对应的 `.hh`
2. 第三方库
3. 标准库
4. 项目内部头文件

每组间空一行。

### Agent 树规则

1. 一个程序有且只有一个主 Agent（`level == 0`）
2. 父节点通过 `std::unique_ptr` 独占子节点所有权
3. `genSubNode()` 返回 `AFS_Agent&` 引用（非拥有），引用在父节点存活期间有效
4. 删除节点时，`unique_ptr` 析构自动递归销毁所有后代节点
5. 子 Agent 继承父 Agent 的已注册工具（`tool_registry_` 拷贝）
6. 所有 Agent 构造时自动初始化默认上下文（系统提示词）
7. 主 Agent 构造时自动调用 `registerTools()` 注册所有已加载工具插件

### 销毁顺序（关键）

`AFS_Agent` 析构必须严格遵循顺序，否则导致 SIGSEGV：

1. `tool_registry_` 中 `std::function` 对象先释放（确保插件 `.so` 中函数指针失效）
2. 再通过 `AFS_PluginManager` 释放插件引用计数（ref=0 时 `dlclose`）

**原因**: 先 `dlclose` 后析构 `std::function` 会访问已卸载的内存。

### 描述性文件约定

- **AGENTS-CN.md**: 代码描述文件，可出现在项目的多层目录中。下级继承上级约定，并补充本目录特有说明。
- **DEV-STATUS.md**: 开发状态文件，仅在必要时出现，记录阶段、已完成、阻塞点和短期决策。
- **TODO.md**: 顶层待办文件，仅出现在项目顶层。任务结构固定为两级：任务和子任务。

### 编写约定

- 所有描述性文件使用中文
- 说明聚焦事实、边界和约定，避免记录容易过期的闲聊式内容
- 下级目录只描述该目录新增信息，不重复顶层已明确的通用规则
- 公共 API 只增加、不删除，保持向后兼容

## 公共 API

插件开发者通过 `core/include/` 使用公共 API，无需依赖 `core/src/` 内部头文件。

详见: **[core/include/AGENTS-CN.md](core/include/AGENTS-CN.md)**

主要内容：
- `afs.hh` — 总入口，包含所有子模块
- `afs/common.hh` — UUID 生成工具（`AFS::uuid8()` / `AFS::uuid16()`）
- `afs/message.hh` — 消息类型（`AFS::Role`、`AFS::Message` 及便捷子类）
- `afs/plugin.hh` — 插件基类（`AFS::Plugin`）、导出宏、ABI 版本
- `afs/tool.hh` — `AFS::ToolSpec`（插件开发者唯一需要的工具类型）

### 插件开发速览

```cpp
#include <afs.hh>

class MyPlugin final : public AFS::Plugin {
public:
    const char* name() const override { return "my_tool"; }
    AFS::PluginType type() const override { return AFS::PluginType::Tool; }
    void start() override {}
    void stop() override {}
    std::vector<ToolCap> toolCapabilities() const override {
        return {{"my_tool", "Does something useful",
                 R"({"type":"object","properties":{"x":{"type":"number"}}})",
                 [](const std::string& input) -> std::string {
                     return R"({"result":42})";
                 }}};
    }
};

AFS_PLUGIN_EXPORT std::uint32_t pluginAbiVersion() { return AFS::PluginAbiVersion; }
AFS_PLUGIN_EXPORT AFS::Plugin* createPlugin() { return new MyPlugin(); }
AFS_PLUGIN_EXPORT void destroyPlugin(AFS::Plugin* p) { delete p; }
```

编译安装后，Agent 启动时自动发现并注册。

## 架构概览

```
用户输入 → main.cc
              │
              ├── argc == 1 → AFS_TuiApp::create(default config) → run()
              ├── argc == 2 → AFS_TuiApp::create(argv[1]) → run()
              └── argc >= 3 → runConsole(argv[1], argv[2])
              ├── AFS_Config::loadFromFile(config.json)
              ├── AFS_PluginManager::instance() → loadFromDirectory(default plugin dir)
              ├── AFS_Agent::createMain() → registerTools()
              │     └── setModel(createModel(config))
              │     └── AFS_Context (消息历史)
              │     └── AFS_ToolRegistry (工具注册)
              │     └── AFS_Loop (状态机，私有成员)
              │
              └── agent->run()
                    └── loop_.run(context_, tool_registry_, *model_, uuid_)
                         │
                         ├── logger.publishStart()          → 写入缓冲区
                         ├── logger.publishAssistantMessage()
                         ├── logger.publishToolResult()
                         └── logger.publishComplete(reply)
                              │
                         main.cc / TUI: logger.poll() → 渲染事件
                              │
                         TUI Shell 模式：Tab 切换，/bin/bash -lc 执行，不写入上下文
                    │
                    ├── [WaitingLLM]  buildRequest(ctx, tools, model_name) → model.chatCompletion()
                    ├── [Executing]   解析 tool_calls → registry.execute()
                    └── [Finished]    返回最终回复
```

## 子文档索引

| 目录 | AGENTS-CN.md | 内容 |
|------|-------------|------|
| `./` | 本文件 | 项目概览、快速开始、目录地图、通用约定 |
| `core/` | `core/AGENTS-CN.md` | 核级说明 |
| `core/include/` | `core/include/AGENTS-CN.md` | 公共 API 文档（类型定义、插件开发） |
| `core/src/` | `core/src/AGENTS-CN.md` | 源码分层约定 |
| `core/src/basic/` | `core/src/basic/AGENTS-CN.md` | 基础设施模块概览 |
| `core/src/basic/config/` | `core/src/basic/config/AGENTS-CN.md` | AFS_Config 配置模块 |
| `core/src/basic/models/` | `core/src/basic/models/AGENTS-CN.md` | 模型抽象层（DeepSeek、OpenAI-compatible） |
| `core/src/agent/` | `core/src/agent/AGENTS-CN.md` | AFS_Agent 类、树状结构、所有权模型 |
| `core/src/agent/loop/` | `core/src/agent/loop/AGENTS-CN.md` | AFS_Loop 对话循环（boost::sml 状态机） |
| `core/src/agent/context/` | `core/src/agent/context/AGENTS-CN.md` | AFS_Context 上下文管理器 |
| `core/src/agent/tool/` | `core/src/agent/tool/AGENTS-CN.md` | 工具注册与执行模块 |
| `core/src/plugins/` | `core/src/plugins/AGENTS-CN.md` | 插件系统（PluginManager、生命周期） |
| `core/src/tui/` | `core/src/tui/AGENTS-CN.md` | FTXUI 终端界面（TUI 前端） |
| `core/src/tui/agent/` | `core/src/tui/agent/AGENTS-CN.md` | TUI 与 Agent/Logger/Plugin/Model 的交互桥接 |
| `core/src/tui/input/` | `core/src/tui/input/AGENTS-CN.md` | TUI Enter/Tab/Esc、modified Enter、滚动处理 |
| `core/src/tui/layout/` | `core/src/tui/layout/AGENTS-CN.md` | TUI 英文状态栏、消息区、输入栏布局样式 |
| `core/src/tui/message/` | `core/src/tui/message/AGENTS-CN.md` | TUI 消息模型（role/content/detail） |
| `plugins/` | `plugins/AGENTS-CN.md` | 插件编译、命名规则、目录结构 |
| `plugins/tools/bash/` | `plugins/tools/bash/AGENTS-CN.md` | bash 命令执行工具插件文档 |
| `plugins/tools/compute/` | `plugins/tools/compute/AGENTS-CN.md` | compute 工具插件文档 |

## 当前状态

项目处于 Agent 核开发阶段。**已完成**：配置模块、模型抽象层、Agent 树结构、插件系统、对话循环状态机。**进行中**：Agent 核最小闭环的完善。

详见 [DEV-STATUS.md](DEV-STATUS.md) 和 [TODO.md](TODO.md)。
