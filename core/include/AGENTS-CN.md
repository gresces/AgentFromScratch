# core > include > AGENTS-CN.md

插件开发者使用的公共头文件，所有类型统一在 `AFS` 命名空间下。**这是唯一权威的类型定义文档。**

## 快速开始

```cpp
#include <afs.hh>

class MyPlugin final : public AFS::Plugin {
    const char* name() const override { return "my_plugin"; }
    AFS::PluginType type() const override { return AFS::PluginType::Tool; }
    void start() override {}
    void stop() override {}
    std::vector<ToolCap> toolCapabilities() const override {
        return {{"my_tool", "description", "{}",
                 [](const std::string& in) { return "{}"; }}};
    }
};

AFS_PLUGIN_EXPORT uint32_t pluginAbiVersion() { return AFS::PluginAbiVersion; }
AFS_PLUGIN_EXPORT AFS::Plugin* createPlugin() { return new MyPlugin(); }
AFS_PLUGIN_EXPORT void destroyPlugin(AFS::Plugin* p) { delete p; }
```

编译: `c++ -std=c++23 -fPIC -shared -I<prefix>/include -o ToolPluginMy my_plugin.cpp`
安装: `cp ToolPluginMy ${XDG_CONFIG_HOME:-~/.config}/afs/plugins/tool/`

---

这个文件夹中的头文件只包含导出给开发者的部分，例如工具插件开发者仅需知道 `AFS::ToolSpec`。
`AFS_ToolCall`、`AFS_ToolResult`、`AFS_ToolRegistry` 等运行时类型属于 Agent 内部实现，定义在 `core/src/agent/tool/`。

## 文件

| 文件 | 职责 |
|------|------|
| `afs.hh` | 总入口，包含所有子模块 |
| `afs/common.hh` | UUID 生成工具（header-only） |
| `afs/message.hh` | 消息类型声明 |
| `afs/metadata.hh` | 公共 metadata 辅助函数（`appendMeta`，header-only） |
| `afs/plugin.hh` | 插件基类、导出宏、ABI 版本、`ToolCap`、可选配置 schema 导出签名 |
| `afs/tool.hh` | `AFS::ToolSpec`（插件开发者唯一需要的工具类型） |

所有公共 API 类型均为 header-only，`print()` 方法内联在对应头文件中，无需单独的 `.cc` 实现文件。

---

## `afs/common.hh` — 通用工具

### `AFS::uuid8()`

```cpp
std::string uuid8();
```

8 位自增十六进制 UUID（`%08x`，`00000000` ~ `ffffffff`），静态计数器保证进程内唯一。
Agent 内部使用，插件开发者通常不需要。

### `AFS::uuid16()`

```cpp
std::string uuid16();
```

16 位随机十六进制 UUID（`%016lx`），`std::mt19937_64` 引擎。
工具调用时自动生成，插件开发者通常不需要。

---

## `afs/message.hh` — 消息类型

LLM 交互的统一消息格式，插件开发者可能需要构造响应消息。

### `AFS::Role`

```cpp
enum class Role { System, Developer, User, Assistant, Tool };
```

| 角色 | 含义 | 使用场景 |
|------|------|---------|
| `System` | 系统指令 | Agent 初始化时自动添加 |
| `Developer` | 开发者指令 | 工具提示词 |
| `User` | 用户输入 | 用户消息 |
| `Assistant` | 模型输出 | LLM 响应 |
| `Tool` | 工具结果 | 工具执行返回 |

### `AFS::Message`

```cpp
struct Message {
    Role role;
    std::string content;
    std::optional<std::string> name;
    std::optional<std::string> tool_call_id;
    std::unordered_map<std::string, std::string> metadata;

    std::string print() const;
};
```

### 便捷子类

预设 `role` 的结构体，简化常见消息构造：

| 类型 | 预设 role | 示例 |
|------|----------|------|
| `SystemMessage("...")` | `System` | 系统指令 |
| `DeveloperMessage("...")` | `Developer` | 工具说明 |
| `UserMessage("...")` | `User` | 用户输入 |
| `AssistantMessage("...")` | `Assistant` | 模型输出 |
| `ToolMessage("...")` | `Tool` | 工具结果 |

### `print()` 输出

```
[System] You are a helpful assistant.
[User name=gresces] What is the weather?
[Tool call_id=tc_001] {"temperature": 22} {source=weather_api}
```

必显 `[Role]`，可选附加 `name=`、`call_id=`，有 metadata 时追加 `{key=val, ...}`。

---

## `afs/metadata.hh` — 公共 metadata 辅助

header-only，提供 `appendMeta()` 函数，供 `Message::print()`、`ToolSpec::print()`
及内部运行时类型共用。

```cpp
using MetaData = std::unordered_map<std::string, std::string>;

inline void appendMeta(std::string& out, const MetaData& meta) {
    if (meta.empty()) return;
    out += " {";
    bool first = true;
    for (const auto& [key, value] : meta) {
        if (!first) out += ", ";
        out += key + "=" + value;
        first = false;
    }
    out += "}";
}
```

---

## `afs/plugin.hh` — 插件接口

插件开发者**必须**了解的全部内容。

### 常量与宏

```cpp
inline constexpr std::uint32_t PluginAbiVersion = 1;  // ABI 版本

// 跨平台导出宏
#if defined(_WIN32)
#define AFS_PLUGIN_EXPORT extern "C" __declspec(dllexport)
#else
#define AFS_PLUGIN_EXPORT extern "C" __attribute__((visibility("default")))
#endif
```

### `AFS::PluginType`

```cpp
enum class PluginType : std::uint32_t { Generic = 0, Tool = 1, Skill = 2 };
```

### `AFS::Plugin`

所有插件的抽象基类：

```cpp
class Plugin {
public:
    virtual ~Plugin();
    virtual const char* name() const = 0;           // 插件名
    virtual PluginType type() const = 0;             // 插件类型
    virtual const char* version() const;             // 版本（默认 "1.0.0"）
    virtual void start() = 0;                        // 启动
    virtual void stop() = 0;                         // 停止
};

struct ToolCap {
    std::string name;                                // 能力名称
    std::string description;                         // 功能描述
    std::string input_schema;                        // JSON Schema
    std::function<std::string(const std::string&)> func; // 执行函数
};
virtual std::vector<ToolCap> toolCapabilities() const;  // 默认空列表
```

### C ABI 导出签名

```cpp
using AbiVersionFn    = std::uint32_t (*)();
using CreateFn        = Plugin* (*)();
using DestroyFn       = void (*)(Plugin*);
using ConfigSchemasFn = const char* (*)();  // 可选：返回配置 schema JSON 数组
```

插件必须导出三个符号。

插件可以额外导出 `pluginConfigSchemas()`。宿主只通过 `dlsym` 探测该符号；未导出时按旧插件处理。返回值应是静态生命周期的 UTF-8 JSON 字符串，形状为数组：

```json
[{"module":"plugin.tool.example","path":["plugins","tool","example"],"is_array":false,"fields":[{"name":"enabled","type":"boolean","required":false}]}]
```

### 完整示例

```cpp
#include <afs.hh>

class MyPlugin final : public AFS::Plugin {
public:
    const char* name() const override { return "my_tool"; }
    AFS::PluginType type() const override { return AFS::PluginType::Tool; }
    void start() override {}
    void stop() override {}

    std::vector<ToolCap> toolCapabilities() const override {
        return {
            {"my_tool", "Does something useful",
             R"({"type":"object","properties":{"x":{"type":"number"}}})",
             [](const std::string& input) -> std::string {
                 return R"({"result":42})";
             }},
        };
    }
};

AFS_PLUGIN_EXPORT std::uint32_t pluginAbiVersion() { return AFS::PluginAbiVersion; }
AFS_PLUGIN_EXPORT AFS::Plugin* createPlugin() { return new MyPlugin(); }
AFS_PLUGIN_EXPORT void destroyPlugin(AFS::Plugin* p) { delete p; }
```

编译：`c++ -std=c++23 -fPIC -shared -fvisibility=hidden my_plugin.cpp -I<core/include> -o ToolPluginMy`

---

## `afs/tool.hh` — `AFS::ToolSpec`

插件开发者唯一需要知道的工具类型。

```cpp
struct ToolSpec {
    std::string name;
    std::string description;
    std::string input_schema;
    std::unordered_map<std::string, std::string> metadata;
    std::string print() const;
};
```

`AFS::ToolSpec` 是 `AFS::Plugin::ToolCap` 的公共表示。插件通过 `ToolCap` 返回能力，
Agent 内部转换为 `ToolSpec` 注册到 `AFS_ToolRegistry`。

---

## 完整插件开发流程

```
1. 创建源文件 my_plugin.cpp
2. #include <afs.hh>
3. 继承 AFS::Plugin，实现纯虚方法
4. 覆写 toolCapabilities() 返回能力列表
5. 导出三个 C ABI 符号
6. 编译: c++ -std=c++23 -fPIC -shared ... -o ToolPluginMy
7. 安装到 `${XDG_CONFIG_HOME:-~/.config}/afs/plugins/tool/`
8. Agent 启动时自动加载并注册
```

## 编译

插件开发者只需包含 `core/include/` 或 `xmake install` 后的 `<prefix>/include/` 目录，无需依赖 `core/src/` 内部头文件。

## 约定

- 公共 API 只增加、不删除，保持向后兼容。
- 运行时类型（`AFS_ToolCall`、`AFS_ToolResult`、`AFS_ToolRegistry`）不在此目录。
- 所有 `print()` 实现位于 `core/src/afs/`。
