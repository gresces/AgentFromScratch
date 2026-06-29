# AgentFromScratch — Core（Agent 核心引擎）

Agent 运行时核心，包含 Agent 树管理、LLM 交互循环、工具插件系统、配置与模型抽象。
所有源文件位于 `src/`，公共头文件位于 `include/`，构建产物输出到 `../bin/`。

---

## 目录结构

```
core/
├── xmake.lua                  # 构建配置
├── .clang-tidy                # clang-tidy 配置（仅检查未使用 include）
├── .gitignore                 # 忽略 bin/、.xmake/、build/
├── compile_commands.json      # 自动生成的编译数据库（LSP 用）
│
├── include/                   # 公共 API 头文件（插件开发者只需依赖此目录）
│   ├── AGENTS-CN.md           # 公共 API 详细文档（类型定义、插件接口、使用示例）
│   ├── afs.hh                 # 总入口，包含所有子模块
│   └── afs/                   # 按模块分组的公共头文件
│       ├── common.hh          # UUID 生成工具
│       ├── message.hh         # 消息类型（AFS::Message、AFS::Role）
│       ├── metadata.hh        # 公共 metadata 辅助函数（appendMeta）
│       ├── plugin.hh          # 插件基类、导出宏、ABI 版本
│       └── tool.hh            # AFS::ToolSpec
│
└── src/                       # 源代码
    ├── AGENTS-CN.md           # 源码目录概览
    ├── main.cc                # 程序入口：配置加载、插件加载、Agent 创建与运行
    │
    ├── basic/                 # 基础设施模块
    │   ├── AGENTS-CN.md
    │   ├── config/            # JSON 配置加载（AFS_Config、AFS_ModelConfig）
    │   └── models/            # LLM 模型抽象（OpenAI 兼容协议、DeepSeek）
    │
    ├── agent/                 # Agent 核心定义与运行循环
    │   ├── AGENTS-CN.md       # AFS_Agent 类、树结构、所有权模型、生命周期
    │   ├── agent.hh / .cc     # Agent 节点实现
    │   ├── context/           # 对话上下文管理（AFS_Context）
    │   ├── loop/              # LLM 交互循环（boost::sml 状态机）
    │   └── tool/              # 工具调用框架（AFS_ToolRegistry）
    │
    ├── plugins/               # 插件系统
    │   ├── AGENTS-CN.md       # 插件发现、加载、引用计数、生命周期
    │   ├── plugin_manager.hh/.cc  # 单例管理器
    │   └── plugin_loader.hh/.cc   # dlopen/dlsym/dlclose RAII 封装
    │
    └── include/               # 内部兼容包装（已迁移至 core/include/，仅保留引用）
```

---

## 构建系统

### xmake

项目使用 **xmake** 作为构建工具，配置文件为 `xmake.lua`。

```lua
add_rules("mode.debug", "mode.release")
add_rules("plugin.compile_commands.autoupdate", { outputdir = "$(projectdir)" })
set_targetdir("$(projectdir)/../bin")
```

**两种构建模式**：

| 模式 | 命令 | 编译选项 |
|------|------|---------|
| Debug | `xmake f -m debug && xmake` | 无优化，含调试符号 |
| Release | `xmake f -m release && xmake` | `-O3 -DNDEBUG` |

**构建产物**：输出到 `core/../bin/`（即项目根目录下的 `bin/`）。

### compile_commands.json

通过 xmake 规则 `plugin.compile_commands.autoupdate` **自动生成与更新**。
每次编译后自动刷新 `core/compile_commands.json`，供 clangd / clang-tidy 等 LSP 工具使用。
**无需手动管理**，不要提交到版本控制（已在 `.gitignore` 中）。

### clang-tidy

`.clang-tidy` 仅配置一条规则：
```yaml
Checks: "-unused-includes"
```
其他检查由 CI 或开发者按需配置。

---

## 依赖库

所有依赖通过 xmake 的 `add_requires` 声明，自动下载与管理：

| 依赖 | 版本 | 用途 | 使用位置 |
|------|------|------|---------|
| `nlohmann_json` | — | JSON 解析与构造 | 配置加载、模型请求/响应、工具参数 |
| `ftxui` | v6.1.9 | TUI 终端界面（预留） | 待集成 |
| `boost_sml` | v1.1.13 | C++ 状态机 | Agent 主循环（`loop/`） |
| `taskflow` | v4.0.0 | 并行任务编排（预留） | 待集成 |
| `cpr` | v1.14.2 | HTTP 客户端（封装 libcurl） | 模型 API 调用（`basic/models/`） |

链接的系统库：`dl`（动态链接库加载，供插件系统使用）。

---

## C++23 标准

项目**仅支持 C++23**，编译选项硬编码 `-std=c++23`。

```lua
set_languages("c++23")
```

使用的 C++23 特性：
- `std::print` / `std::println`（`<print>`）— 在 `main.cc` 中使用
- `std::unreachable()` — 语义标注
- `std::expected`（`<expected>`）— 错误处理
- `std::optional` 增强（monadic operations）
- 其他 C++20/23 特性按需使用

---

## 构建 / 运行 / 测试流程

### 构建

```bash
cd core

# Debug 构建
xmake f -m debug
xmake

# Release 构建
xmake f -m release
xmake

# 清理
xmake clean
```

### 运行

```bash
# Agent 二进制位于 bin/
../bin/Agent <config.json> <prompt>

# 示例
../bin/Agent ../config.json "What is 2+2? Compute it."
```

**参数说明**：
- `config.json`：JSON 配置文件，包含 LLM 模型配置（格式见 `src/basic/config/AGENTS-CN.md`）
- `prompt`：用户提示词

### 构建插件

插件独立编译，仅需包含 `core/include/`：

```bash
c++ -std=c++23 -fPIC -shared -fvisibility=hidden \
    my_plugin.cpp \
    -I core/include \
    -o ToolPluginMy
```

插件需放入 `bin/plugins/tool/`（工具插件）或 `bin/plugins/skill/`（技能插件），
文件命名格式：`<Type>Plugin<Name>`（详见 `src/plugins/AGENTS-CN.md`）。

### 典型开发工作流

```
1. 修改源码 (src/*.cc, src/*.hh)
2. xmake build         ← 增量编译
3. ../bin/Agent ...    ← 运行验证
4. （可选）编译/更新插件到 bin/plugins/
```

---

## 代码约定

| 规则 | 说明 |
|------|------|
| 源文件扩展名 | `.cc`（实现）、`.hh`（头文件） |
| 头文件保护 | `#pragma once` |
| 内部符号可见性 | `-fvisibility=hidden -fvisibility-inlines-hidden` |
| 插件符号可见性 | 显式 `AFS_PLUGIN_EXPORT` 导出 |
| 命名空间 | 公共 API → `AFS`；内部实现 → `AFS_` 前缀（如 `AFS_Agent`） |
| 格式化规范 | 遵循 `../coding-standards/language-rules/cpp/format.md` |

---

## 子文档索引

各子目录有独立的 `AGENTS-CN.md`，包含模块职责边界、API 详解和使用示例：

| 文档 | 内容 |
|------|------|
| `include/AGENTS-CN.md` | 公共 API 完整文档：消息类型、插件接口、使用示例 |
| `src/AGENTS-CN.md` | 源码目录概览与通用约定 |
| `src/basic/AGENTS-CN.md` | 基础设施模块导航 |
| `src/basic/config/AGENTS-CN.md` | 配置加载、JSON 格式 |
| `src/basic/models/AGENTS-CN.md` | 模型抽象、HTTP 请求、工厂函数 |
| `src/agent/AGENTS-CN.md` | Agent 树结构、所有权模型、生命周期 |
| `src/agent/context/AGENTS-CN.md` | 对话上下文管理 |
| `src/agent/loop/AGENTS-CN.md` | 状态机循环、LLM 交互 |
| `src/agent/tool/AGENTS-CN.md` | 工具注册与执行 |
| `src/plugins/AGENTS-CN.md` | 插件系统、引用计数、销毁顺序 |
