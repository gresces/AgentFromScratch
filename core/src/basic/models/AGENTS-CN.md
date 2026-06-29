# core > src > basic > models > AGENTS-CN.md

Agent 运行所需的模型抽象层，当前支持对话模型（LLM）和嵌入模型（embedding），后续可扩展其他模型类型。

网络请求通过 `cpr` 库实现（封装 libcurl）。

## 文件

| 文件 | 职责 |
|------|------|
| `model.hh` | 所有类声明：`AFS_Model` 基类、`AFS_Model_OpenAICompatible`、`AFS_Model_DeepSeek`、工厂函数 |
| `model.cc` | 所有实现：HTTP 请求、JSON 解析、工厂函数 |

## 类层次

```
AFS_Model                         (抽象基类)
  └── AFS_Model_OpenAICompatible  (OpenAI 兼容协议)
        └── AFS_Model_DeepSeek    (DeepSeek API)
```

### `AFS_Model`

不可变抽象基类，由 `AFS_ModelConfig` 创建，构造后所有成员数据不可修改（深拷贝语义）。

| 方法 | 返回 | 说明 |
|------|------|------|
| `modelType()` | `std::string_view` | 模型类型标识，子类覆盖，用于区分协议 |
| `chatCompletion(request)` | `std::optional<nlohmann::json>` | 聊天补全请求，失败返回 `nullopt` |
| `embedding(request)` | `std::optional<nlohmann::json>` | 嵌入向量请求，失败返回 `nullopt` |
| `name()` | `const std::string&` | 模型标识名（来自配置的 `name` 字段） |
| `modelName()` | `std::string` | API 模型名称（来自配置的 `model` 字段），子类覆盖 |

### `AFS_Model_OpenAICompatible`

兼容 OpenAI API 协议的通用实现，`modelType()` 返回 `"OpenAICompatible"`。

- 构造时从 `AFS_ModelConfig` 深拷贝 `base_url`、`api_key`、`model`。
- `chatCompletion()` → POST `{base_url}/chat/completions`
- `embedding()` → POST `{base_url}/embeddings`
- 请求 body 中若未指定 `"model"` 字段，自动填入配置中的 `model` 值。
- 请求失败（网络错误、非 2xx 状态码、响应非合法 JSON）统一返回 `std::nullopt`。
- `base_url_`、`api_key_`、`model_` 为 `protected` 字段，子类可直接访问。

### `AFS_Model_DeepSeek`

专用于 DeepSeek API 的模型，`modelType()` 返回 `"DeepSeek"`。

- 继承 `AFS_Model_OpenAICompatible`，复用 OpenAI 兼容的请求/响应格式。
- 构造时直接将 `AFS_ModelConfig` 委托给父类，无额外逻辑。
- DeepSeek API 与 OpenAI 协议完全兼容，端点路径、请求/响应格式均相同。
- 单独设为一个子类的理由：未来 DeepSeek 可能有专有特性（如特殊的 reasoning token 处理），独立子类便于定制。
- 参考文档：[DeepSeek API Docs](https://api-docs.deepseek.com/)

## 工厂函数

```cpp
std::unique_ptr<AFS_Model> createModel(const AFS_ModelConfig& cfg);
```

根据 `base_url` 自动识别协议类型并返回对应的子类实例：

| base_url 特征 | 创建的类 | modelType |
|---|---|---|
| 包含 `api.deepseek.com` | `AFS_Model_DeepSeek` | `"DeepSeek"` |
| 其他 | `AFS_Model_OpenAICompatible` | `"OpenAICompatible"` |

工厂使用 `std::make_unique` 构造，返回值为独占所有权指针。调用方接管生命周期。

## 使用示例

### 创建模型并发送聊天请求

```cpp
#include "basic/models/model.hh"
#include "basic/config/config.hh"

// 从配置创建模型
AFS_ModelConfig cfg;
cfg.name = "DeepSeek";
cfg.base_url = "https://api.deepseek.com";
cfg.api_key = "sk-xxxx";
cfg.model = "deepseek-v4-pro";

auto model = createModel(cfg);
assert(model->modelType() == "DeepSeek");

// 构造聊天请求
nlohmann::json req;
req["messages"] = nlohmann::json::array({
    {{"role", "system"}, {"content", "你是一个有用的助手。"}},
    {{"role", "user"},   {"content", "解释量子计算的基本原理。"}}
});
req["temperature"] = 0.7;

// 发送请求
auto resp = model->chatCompletion(req);
if (resp) {
    // 解析响应
    auto& choice = (*resp)["choices"][0];
    std::string content = choice["message"]["content"];
    std::cout << content << std::endl;
} else {
    std::cerr << "请求失败" << std::endl;
}
```

### 遍历多个模型发送请求

```cpp
auto config = AFS_Config::loadFromFile("config.json");
if (!config) return;

for (const auto& llm_cfg : config->models().llms) {
    auto model = createModel(llm_cfg);

    nlohmann::json req;
    req["messages"] = nlohmann::json::array({
        {{"role", "user"}, {"content", "你好"}}
    });

    auto resp = model->chatCompletion(req);
    if (resp) {
        std::cout << "[" << model->name() << "] "
                  << (*resp)["choices"][0]["message"]["content"]
                  << std::endl;
    }
}
```

### 发送 embedding 请求

```cpp
auto model = createModel(embedding_cfg);

nlohmann::json req;
req["input"] = "需要计算嵌入向量的文本";

auto resp = model->embedding(req);
if (resp) {
    auto& embedding = (*resp)["data"][0]["embedding"];
    // embedding 是 float 数组
}
```

## 设计说明

### HTTP 请求层

HTTP 请求逻辑（`postJson`）提取为 `model.cc` 文件内匿名命名空间的自由函数：

```cpp
namespace {
std::optional<nlohmann::json> postJson(
    const std::string& url,
    const std::string& api_key,
    const nlohmann::json& body);
}
```

- 职责单一：发 POST、带 Bearer auth、解析 JSON 响应。
- 网络错误、非 2xx 状态码、JSON 解析失败统一返回 `nullopt`。
- 不记录日志、不重试、不流式处理——这些由上层决定。

### 扩展模型

新增模型协议分为三种情况：

1. **兼容 OpenAI 协议的新服务**（如 `api.moonshot.cn`）  
   在 `createModel` 中增加 `base_url` 匹配规则，返回 `AFS_Model_OpenAICompatible` 实例即可，无需新增类。

2. **兼容 OpenAI 协议但需要定制行为**（如 DeepSeek 当前状态）  
   继承 `AFS_Model_OpenAICompatible`，覆盖 `modelType()` 返回专用标识，后续可按需覆盖 `chatCompletion()` / `embedding()`。

3. **完全不兼容的协议**（如 Anthropic Messages API、原生 Gemini API）  
   直接继承 `AFS_Model`，实现全部虚函数，编写独立的 HTTP 请求逻辑。

### 文件组织

- 所有模型类型声明集中在 `model.hh`，实现在 `model.cc`。不按协议类型拆分为多个头文件。
- 理由：当前规模较小，集中管理避免头文件碎片化。如果未来模型类型超过 5 种，可考虑按协议拆分。

## 依赖

- `nlohmann_json` — JSON 构造与解析（header-only）。
- `cpr` — HTTP 客户端（封装 libcurl）。
- `basic/config/config.hh` — `AFS_ModelConfig` 结构体。

## 约定

- 模型实例不保存运行时上下文（如对话历史），仅提供无状态的 API 调用能力。每次 `chatCompletion` 调用是独立的。
- 创建后不可变：不提供任何 setter，拷贝/移动均执行深拷贝（继承自 `AFS_Model` 的拷贝控制）。
- 调用方不感知底层 HTTP 细节（cpr、Bearer Token 拼接、端点路径），只需构造 request JSON 并处理 response JSON。
- `chatCompletion` 和 `embedding` 的参数 `request` 为完整 JSON 对象，模型只负责补全缺失的 `"model"` 字段，其余字段（`messages`、`temperature` 等）原样传递。
- 失败处理：所有失败统一返回 `std::nullopt`，不区分错误类型（网络超时、4xx、5xx、JSON 解析错误）。调用方如需区分错误类型，需扩展返回值设计。
- 子类化时：`AFS_Model_OpenAICompatible` 的 `protected` 字段（`base_url_`、`api_key_`、`model_`）可直接使用，无需通过 getter。

## 注意事项

- **`model` 字段的自动填充**：`chatCompletion` 和 `embedding` 仅在 request JSON 中**不存在** `"model"` 键时才自动填入。如果调用方已经设置了 `"model"`（或许是覆盖配置），则保留调用方的值。
- **base_url 拼接**：端点路径直接通过 `+` 拼接（`base_url_ + "/chat/completions"`），不在中间插入 `/`。确保 `base_url` 不含尾部 `/`，否则可能出现双斜杠。
- **只返回 JSON 不返回 HTTP 状态码**：当前返回值仅包含解析后的 JSON body。调用方无法区分 "200 但业务错误" 和 "网络超时"——两者都得到 `nullopt`。生产环境如需详细错误信息，需修改接口。
- **线程安全**：模型实例不可变（所有成员 `const` 语义），`chatCompletion` / `embedding` 方法仅读取成员并发送网络请求，天然线程安全。多个线程可共享同一个 `AFS_Model` 实例。
- **cpr 超时**：当前未设置 cpr 的超时参数（使用 libcurl 默认超时）。长时间无响应的请求可能阻塞调用线程。后续可考虑增加 `AFS_ModelConfig` 中的超时配置字段。
- **无流式支持**：当前 `chatCompletion` 返回完整 JSON 响应，不支持 `stream: true` 的分块处理。流式调用需扩展接口。
