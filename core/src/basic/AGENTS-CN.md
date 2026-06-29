# core > src > basic > AGENTS-CN.md

Agent 核心的基础设施模块目录，提供配置加载、模型抽象、日志、错误处理等通用能力。

## 子目录

| 目录 | 职责 |
|------|------|
| `config/` | 全局配置加载，见 `config/AGENTS-CN.md` |
| `models/` | 模型抽象与 API 调用，见 `models/AGENTS-CN.md` |

## 模块关系

```
config/                        models/
  │                              │
  ├── AFS_Config                 ├── AFS_Model（抽象基类）
  │     └── AFS_ModelsConfig     │     ├── AFS_Model_OpenAICompatible
  │           ├── AFS_ModelConfig│     │     └── AFS_Model_DeepSeek
  │           │   (传递给工厂) ──┼──→ createModel(cfg)
  │           └── AFS_ModelConfig│
  └── loadFromFile(path)         └── chatCompletion / embedding
```

核心流程：`AFS_Config::loadFromFile()` 解析 JSON → 取出 `AFS_ModelConfig` → `createModel()` 工厂创建模型实例 → 调用 `chatCompletion()` / `embedding()`。

## 代码示例

### 完整加载配置并创建模型

```cpp
#include "basic/config/config.hh"
#include "basic/models/model.hh"

// 1. 加载配置
auto config = AFS_Config::loadFromFile("config.json");
if (!config) {
    // 处理加载失败：文件不存在、JSON 语法错误、缺少必填字段
    return;
}

// 2. 遍历 LLM 模型列表，创建模型实例
for (const auto& llm_cfg : config->models().llms) {
    auto model = createModel(llm_cfg);
    // model->modelType() 返回 "DeepSeek" 或 "OpenAICompatible"

    // 3. 发送聊天请求
    nlohmann::json req;
    req["messages"] = nlohmann::json::array({
        {{"role", "user"}, {"content", "你好"}}
    });
    auto resp = model->chatCompletion(req);
    if (resp) {
        // 处理响应...
    }
}
```

## 约定

- 本目录不依赖展示层代码（UI、终端渲染等）。
- 各子模块通过 `.hh` 头文件暴露公共接口，`.cc` 文件实现细节，匿名命名空间内的函数视为文件内部实现。
- 模块间通过 `AFS_Config`、`AFS_ModelConfig` 等明确值类型传递数据，不使用全局变量或单例。
- `config/` 和 `models/` 是基础层最底部的两个模块，其他模块（如 agent 逻辑、工具调用）依赖它们，反之不行。

## 注意事项

- `loadFromFile` 遇到任何异常（文件打不开、JSON 解析失败、必填字段缺失）统一返回 `std::nullopt`，调用方自行决定错误提示策略。不抛出异常。
- 模型工厂 `createModel` 通过 `base_url` 字符串匹配识别协议，新增协议时只需在工厂函数增加一个 `if` 分支 + 对应的子类实现。
- 当前版本配置文件仅支持 JSON 格式，不支持 YAML/TOML，也不支持命令行参数覆盖——这些后续可能扩展。
