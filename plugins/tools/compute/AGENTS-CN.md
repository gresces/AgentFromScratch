# plugins > tools > compute > AGENTS-CN.md

数值计算工具插件，作为插件开发完整样例。

## 文件

| 文件 | 职责 |
|------|------|
| `compute.cpp` | 插件源码（~80 行） |
| `build.sh` | 编译/安装/清理脚本 |

## 功能

二元加减乘除运算，通过 `AFS::Plugin::toolCapabilities()` 暴露。

**输入格式**（JSON 字符串）：
```json
{"op": "add", "a": 3, "b": 5}
```

| op 值 | 运算 | 示例 | 结果 |
|-------|------|------|------|
| `add` | a + b | `{"op":"add","a":3,"b":5}` | `{"result":8}` |
| `sub` | a - b | `{"op":"sub","a":10,"b":3}` | `{"result":7}` |
| `mul` | a * b | `{"op":"mul","a":4,"b":5}` | `{"result":20}` |
| `div` | a / b | `{"op":"div","a":10,"b":2}` | `{"result":5}` |
| `div` | 除零 | `{"op":"div","a":10,"b":0}` | `{"error":"division by zero"}` |

## 代码结构

```cpp
#include <afs.hh>          // 公共 API 头文件

class ComputePlugin final : public AFS::Plugin {
    // name(), type(), start(), stop()

    std::vector<ToolCap> toolCapabilities() const override {
        return {
            {"compute",
             "Binary arithmetic: add, sub, mul, div. ...",
             R"({...json schema...})",
             [](const std::string& input) -> std::string {
                 // 解析 JSON → 执行运算 → 返回结果
             }},
        };
    }
};

// C ABI 导出
AFS_PLUGIN_EXPORT uint32_t pluginAbiVersion() { ... }
AFS_PLUGIN_EXPORT AFS::Plugin* createPlugin() { ... }
AFS_PLUGIN_EXPORT void destroyPlugin(AFS::Plugin* p) { ... }
```

## 编译

```sh
# 从顶层编译
cd plugins && ./build.sh compute && ./build.sh compute install

# 或单独编译
cd plugins/tools/compute
./build.sh          # → ToolPluginCompute
./build.sh install  # → 安装到 bin/plugins/tool/
./build.sh clean    # 清理
```

编译依赖：C++23 编译器，`core/include/` 公共头文件。JSON 解析内嵌轻量实现，无需外部库。

## 安装后验证

```sh
# 启动 Agent 后查看
--- 插件管理器 ---
已加载工具插件: 1 个
  [ToolSpec] compute: Binary arithmetic: add, sub, mul, div. ...

# 执行工具
[ToolCall ...] compute({"op":"add","a":3,"b":5})
[ToolResult ...] compute OK: {"result":8}
```
