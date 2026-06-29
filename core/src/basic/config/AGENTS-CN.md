# core > src > basic > config > AGENTS-CN.md

Agent 核心所需的配置模块，通过 JSON 配置文件确定运行参数。

## 文件

- `config.hh` — `AFS_Config` 类声明及 `AFS_ModelConfig`、`AFS_ModelsConfig` 结构体定义。
- `config.cc` — `AFS_Config::loadFromFile` 实现及 `nlohmann::json` 反序列化适配。
- `paths.hh` / `paths.cc` — 用户配置目录、默认配置文件、默认插件目录的路径解析。

## 结构体定义

### `AFS_ModelConfig`

单个模型配置条目，LLM 和 embedding 模型共用同一结构。

| 字段 | 类型 | 说明 |
|------|------|------|
| `name` | `std::string` | 模型标识名（用于日志/调试，非 API 参数） |
| `base_url` | `std::string` | API 基地址（不含尾部路径，如 `https://api.deepseek.com`） |
| `api_key` | `std::string` | API 密钥（Bearer Token） |
| `model` | `std::string` | 模型名称（传递给 API 的 `model` 字段） |

### `AFS_ModelsConfig`

模型分组，包含 LLM 列表和 embedding 模型列表。

| 字段 | 类型 | 说明 |
|------|------|------|
| `llms` | `std::vector<AFS_ModelConfig>` | LLM 模型列表，支持多个 |
| `embeddings` | `std::vector<AFS_ModelConfig>` | embedding 模型列表，支持多个 |

### `AFS_Config`

Agent 全局配置。不持有文件路径或原始 JSON，只保留解析后的结构化数据。当前版本仅包含 `models` 段，后续可扩展其他配置段。

| 方法 | 说明 |
|------|------|
| `static loadFromFile(path)` | 从 JSON 文件路径加载，失败返回 `std::nullopt` |
| `models()` | 返回 `AFS_ModelsConfig` 的只读引用 |

## JSON 配置文件格式

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
    ],
    "embeddings": [
      {
        "name": "Bailian",
        "base_url": "https://dashscope.aliyuncs.com/compatible-mode/v1",
        "api_key": "sk-xxxx",
        "model": "text-embedding-v3"
      }
    ]
  }
}
```

### 字段规则

- 顶层 `"models"` 键为可选；缺失时 `AFS_ModelsConfig` 使用空默认值（`llms` 和 `embeddings` 均为空 vector）。
- `"llms"` 和 `"embeddings"` 均为可选；缺失对应列表为空。
- 单条模型配置（`AFS_ModelConfig`）的四个字段（`name`, `base_url`, `api_key`, `model`）均为必须。任一缺失时 `nlohmann::json::at()` 抛出 `out_of_range` 异常，`loadFromFile` 捕获后返回 `std::nullopt`。
- `base_url` **不应**包含尾部 `/` 或 API 路径段（如 `/chat/completions`）。模型实现会自动拼接端点路径。
- 支持配置多个 LLM 和多个 embedding 模型，调用方按 `name` 字段选择。

## 使用示例

### 默认运行时路径

程序默认使用用户配置目录下的 `afs/` 子目录：

| 项 | 路径 |
|----|------|
| 配置目录 | `${XDG_CONFIG_HOME}/afs`；若未设置 `XDG_CONFIG_HOME`，则为 `${HOME}/.config/afs` |
| 默认配置文件 | `${XDG_CONFIG_HOME:-$HOME/.config}/afs/config.json` |
| 默认插件目录 | `${XDG_CONFIG_HOME:-$HOME/.config}/afs/plugins/` |

对应 helper：

```cpp
#include "basic/config/paths.hh"

auto config_path = AFS_DefaultConfigPath();
auto plugin_dir = AFS_DefaultPluginDirectory();
```

`Agent` 无参数启动时读取默认配置文件并进入 TUI；显式传入 `<config.json>` 时使用该文件，但插件仍默认从用户配置目录的 `afs/plugins/` 加载。

### 加载配置

```cpp
#include "basic/config/config.hh"

auto cfg = AFS_Config::loadFromFile("config.json");
if (!cfg) {
    std::cerr << "无法加载配置文件" << std::endl;
    return;
}

// 访问 LLM 列表
for (const auto& llm : cfg->models().llms) {
    std::cout << "LLM: " << llm.name << " (" << llm.model << ")" << std::endl;
}

// 访问 embedding 模型列表
for (const auto& emb : cfg->models().embeddings) {
    std::cout << "Embedding: " << emb.name << std::endl;
}
```

### 程序化构造配置（不经过文件）

```cpp
AFS_ModelConfig llm;
llm.name = "OpenAI";
llm.base_url = "https://api.openai.com/v1";
llm.api_key = "sk-...";
llm.model = "gpt-4o";

AFS_Config cfg;
// 注意：AFS_Config 目前不直接提供修改 models_ 的 setter，
// 但 AFS_ModelsConfig 和 AFS_ModelConfig 均为普通 struct，可直接赋值。
```

## 设计决策

- **值语义**：所有配置结构体均为普通值类型（无虚函数、无继承），支持复制和移动。
- **失败即 nullopt**：`loadFromFile` 统一返回 `std::optional`，不抛异常。失败原因包括：文件不存在、JSON 语法错误、必填字段缺失——均返回 `std::nullopt`。调用方自行决定是否打印错误及其粒度。
- **只读访问**：`AFS_Config` 通过 `const&` 返回 `models()`，外部不可修改配置内容。当前版本没有提供修改接口。
- **显式 `from_json`**：反序列化使用 nlohmann 的 `from_json` ADL 定制点，`loadFromFile` 内部不直接拼接字段名，新增字段时只需修改 `from_json` 实现。

## 依赖

- `nlohmann_json` — JSON 解析（header-only）。

## 约定

- `AFS_Config` 不持有文件路径，只保存解析后的结构化数据。
- 配置字段目前均通过 JSON 文件确定，无命令行参数入口。
- 顶层键（如 `models`）缺失时使用空默认值，不报错。
- 单条模型配置的四个字段均为必须；缺失时 `nlohmann::json::at()` 抛出异常，`loadFromFile` 返回 `std::nullopt`。
- 扩展新的配置段（如 `"logging"`、`"tools"`）时：在 `AFS_Config` 中新增成员字段 + 对应的 struct，在 `loadFromFile` 中增加 `root.contains("xxx")` 分支，并编写对应的 `from_json`。

## 注意事项

- `base_url` 末尾不要加 `/`。模型实现会拼接 `"/chat/completions"` 或 `"/embeddings"`。如果 base_url 已经是 `"https://api.openai.com/v1"`（不含尾部 `/`），拼接后为 `"https://api.openai.com/v1/chat/completions"`——正确。如果误写成 `"https://api.openai.com/v1/"`，拼接后出现双斜杠 `//chat/completions`，多数服务端可以容忍但不推荐。
- API 密钥以明文存储于 JSON 文件中，当前版本无加密或环境变量导入机制。生产环境部署时需注意文件权限。
- `AFS_Config` 目前是值类型且不可变，`loadFromFile` 返回的是栈上的值对象（`std::optional<AFS_Config>`），调用方持有所有权。
