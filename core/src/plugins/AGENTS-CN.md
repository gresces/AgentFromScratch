# core > src > plugins > AGENTS-CN.md

插件系统：插件发现、加载、引用计数和生命周期管理。

## 文件

| 文件 | 职责 |
|------|------|
| `plugin_loader.hh` | `AFS_LoadedPlugin` RAII 封装 |
| `plugin_loader.cc` | `dlopen`/`dlsym`/`dlclose` |
| `plugin_manager.hh` | `AFS_PluginManager` 单例 |
| `plugin_manager.cc` | 目录扫描、命名解析、引用计数 |

## `AFS_PluginManager`

全局唯一插件管理器（`std::shared_ptr` 单例）。伴随 Agent Core 启动，负责所有插件的生命周期。

| 方法 | 说明 |
|------|------|
| `instance()` | 获取全局实例 |
| `loadFromDirectory(dir)` | 扫描 `dir/<type>/` 子目录，解析 `<Type>Plugin<Name>` 文件名格式并加载（ref=1） |
| `loadPlugin(type, name)` | 按类型+名称加载指定插件；已加载则 ref+1 |
| `unloadPlugin(type, name)` | 释放引用；ref=0 时自动 dlclose |
| `allToolCaps()` | 获取所有已加载工具插件的 `ToolCap` 列表 |
| `toolCaps(type, name)` | 获取指定插件的 `ToolCap` 列表 |
| `loadedToolPlugins()` | 获取所有已加载工具插件的 `(type, name)` 列表 |

## 引用计数

每个插件维护独立的引用计数，支持多 Agent 共享：

```cpp
pm->loadPlugin(Tool, "compute")   → ref=1   // Agent A 加载
pm->loadPlugin(Tool, "compute")   → ref=2   // Agent B 也加载（共享）
pm->unloadPlugin(Tool, "compute") → ref=1   // Agent A 释放
pm->unloadPlugin(Tool, "compute") → ref=0   // 最后引用释放 → dlclose
```

`AFS_Agent` 析构时自动调用 `unloadPlugin` 释放其加载的所有插件。

## 生命周期约束

**必须严格遵守的销毁顺序**：
1. Agent 先清理 `tool_registry_`（释放 `std::function` 对象）
2. 再调用 `pm->unloadPlugin()`（触发 `dlclose`）

原因：`tool_registry_` 中存储的函数对象指向插件 `.so` 中的代码，
若先 `dlclose` 后析构 `std::function`，会访问已卸载的内存导致 SIGSEGV。

Agent 的 `~AFS_Agent()` 已保证此顺序，无需额外处理。

## 启动流程

```
程序启动
  │
  ├── pm = AFS_PluginManager::instance()
  │
  ├── pm->loadFromDirectory(AFS_DefaultPluginDirectory())
  │     ├── 扫描 ${XDG_CONFIG_HOME:-~/.config}/afs/plugins/tool/ToolPlugin*
  │     ├── 扫描 ${XDG_CONFIG_HOME:-~/.config}/afs/plugins/skill/SkillPlugin*
  │     └── 每个文件: dlopen → 验证 C ABI → 存储 (ref=1)
  │
  ├── agent = AFS_Agent::createMain()
  │     └── 自动调用 registerTools()（level==0）
  │     ├── pm->loadedToolPlugins() → [(Tool,"compute"), ...]
  │     ├── 遍历: pm->toolCaps(type, name)
  │     ├── 每个 cap → tool_registry_.registerTool(spec, lambda)
  │     ├── 记录到 loaded_plugins_ (析构时释放)
  │     └── 追加工具提示到 context_
  │
  ├── agent->genSubNode()
  │     └── 子 Agent 拷贝 tool_registry_ (继承工具)
  │           loaded_plugins_ 为空 (不继承引用)
  │
  └── ~AFS_Agent()
        └── 遍历 loaded_plugins_ → pm->unloadPlugin() → ref--
              └── ref=0 → dlclose
```

## 插件命名规则

编译产物命名格式：`<Type>Plugin<Name>`（无后缀）

| type | name | 文件名 |
|------|------|--------|
| Tool | compute | `ToolPluginCompute` |
| Tool | weather | `ToolPluginWeather` |
| Skill | search | `SkillPluginSearch` |

目录结构：
```
${XDG_CONFIG_HOME:-~/.config}/afs/plugins/
├── tool/
│   ├── ToolPluginCompute
│   └── ToolPluginWeather
└── skill/
    └── SkillPluginSearch
```

## 架构关系

```
AFS_PluginManager                         AFS_Agent
  ├── loadFromDirectory(dir)  ─────────→  registerTools()
  │     ├── 扫描目录                          ├── 遍历 loadedToolPlugins()
  │     └── dlopen + 验证 ABI                 ├── toolCaps() → 注册到 registry
  │                                           └── 记录 loaded_plugins_
  ├── loadPlugin(type,name)  ──────────→  loadExtraTool(type,name)
  │     └── ref+1                             ├── pm->loadPlugin()
  │                                           ├── toolCaps() → 注册
  ├── unloadPlugin(type,name) ←─────────      └── 记录 loaded_plugins_
  │     └── ref-1, ref=0 → dlclose
  │                                    ~AFS_Agent()
  │                                      └── 遍历 loaded_plugins_
  │                                           → pm->unloadPlugin()
  └── (工具能力查询)
        ├── allToolCaps() → Agent 批量注册用
        ├── toolCaps(type,name) → 单个查询
        └── loadedToolPlugins() → 列表遍历
```
