# core > src > agent > tool > AGENTS-CN.md

Agent 内部工具调用模块。提供工具注册、执行和结果管理。

## 文件

| 文件 | 职责 |
|------|------|
| `tool.hh` | `AFS_ToolCall`、`AFS_ToolResult`、`AFS_ToolRegistry` |
| `tool.cc` | 全部实现 |

## 类型详解

### `AFS_ToolSpec`

工具说明书，描述工具的名称、功能和输入格式。公共类型 `AFS::ToolSpec` 的别名。

### `AFS_ToolCall`

Agent 发起的单次工具调用请求。

```cpp
struct AFS_ToolCall {
    std::string uuid;                     // AFS::uuid16() 自动生成
    std::string name;                     // 工具名称
    std::string arguments;                // JSON 参数
    std::vector<std::string> environment; // 环境变量
    std::unordered_map<std::string, std::string> metadata;
};
```

### `AFS_ToolResult`

工具执行结果。

```cpp
struct AFS_ToolResult {
    std::string call_uuid;  // 对应 ToolCall::uuid
    std::string tool_name;
    bool success;           // true=成功, false=失败
    std::string output;     // 成功时输出
    std::string error;      // 失败时错误信息
    std::unordered_map<std::string, std::string> metadata;
};
```

### `AFS_ToolFunc`

```cpp
using AFS_ToolFunc = std::function<AFS_ToolResult(const AFS_ToolCall&)>;
```

### `AFS_ToolRegistry`

工具注册与执行器。

| 方法 | 说明 |
|------|------|
| `registerTool(spec, func)` | 注册工具说明书和执行函数 |
| `execute(call)` | 按 `call.name` 查找并执行；未注册返回 `success=false` + error |
| `hasTool(name)` | 检查工具是否已注册 |
| `listSpecs()` | 获取所有已注册工具的说明书列表 |

`execute()` 自动将 `call.uuid` 填入 `result.call_uuid`。

## 执行流程图

```
Agent 接收 LLM 响应
  │
  ├── 解析 tool_calls
  │     └── 构造 AFS_ToolCall { name, arguments }
  │
  ├── tool_registry_.execute(call)
  │     ├── 查找已注册工具
  │     ├── 找到 → 调用 AFS_ToolFunc → AFS_ToolResult
  │     └── 未找到 → AFS_ToolResult { success=false, error="..." }
  │
  └── 将 result 转换为 AFS::ToolMessage 追加到上下文
```

## 使用示例

```cpp
AFS_ToolRegistry registry;

// 注册工具
registry.registerTool(
    {"compute", "Binary arithmetic",
     R"({"type":"object","properties":{"op":{"type":"string"},"a":{"type":"number"},"b":{"type":"number"}}})"},
    [](const AFS_ToolCall& call) {
        AFS_ToolResult result;
        // 解析 call.arguments, 执行运算
        result.success = true;
        result.output = R"({"result": 8})";
        return result;
    });

// 执行工具
AFS_ToolCall call;
call.name = "compute";
call.arguments = R"({"op":"add","a":3,"b":5})";

auto result = registry.execute(call);
// result.success == true
// result.output == R"({"result": 8})"
```

## 输出格式

```
[ToolCall a1b2c3d4e5f6a7b8] weather({"city": "Beijing"})
[ToolResult a1b2c3d4e5f6a7b8] weather OK: {"temperature":22}
[ToolResult a1b2c3d4e5f6a7b8] nonexistent FAIL: tool not registered: nonexistent
```

## 架构位置

工具注册由 `AFS_Agent::registerTools()` 和 `AFS_Agent::loadExtraTool()` 调用，执行由 Agent 的 LLM 循环驱动。
插件开发者无需了解本模块，只需实现 `AFS::Plugin::toolCapabilities()`。
