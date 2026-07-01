# core > src > basic > AGENTS-CN.md

Agent 核心的基础设施模块目录，提供动态配置、模型抽象、日志等通用能力。

## 子目录

| 目录 | 职责 |
|------|------|
| `config/` | `AFS_Config` 配置类、动态配置管理器、JSON 加载保存、schema 注册表，见 `config/AGENTS-CN.md` |
| `models/` | 模型配置结构、模型抽象与 API 调用，见 `models/AGENTS-CN.md` |
| `log/` | 全局日志与事件发布订阅，见 `log/AGENTS-CN.md` |

## 模块关系

```
config/                                      models/
  │                                            │
  ├── AFS_Config        <────── 读取 typed config ─┤ AFS_LoadModelsConfig(config)
  ├── AFS_ConfigManager <────── 注册/应用模块 ──────┤ AFS_RegisterModelConfigSchemas()
  └── load/save/update JSON + apply callback       ├── AFS_ModelConfig
                                               ├── AFS_ModelsConfig
                                               ├── AFS_Model_OpenAICompatible
                                               └── createModel(cfg)
```

核心流程：程序启动时 `AFS_ConfigManager` 先读取并解析配置文件 → 模块注册 schema 和可选应用回调 → 模块从 `AFS_Config` 解析自己的 typed config → 修改配置时写回文件并按受影响模块应用。

## 代码示例

### 加载模型配置并创建模型

```cpp
#include "basic/config/config.hh"
#include "basic/models/model.hh"

AFS_RegisterModelConfigSchemas();

auto& config = AFS_ConfigManager::instance();
std::string error;
if (!config.loadFromFile("config.json", error)) {
    return;
}

auto models = AFS_LoadModelsConfig(config.config());
if (!models || models->llms.empty()) {
    return;
}

auto model = createModel(models->llms[0]);
```

## 约定

- 本目录不依赖展示层渲染代码。
- `config/` 是动态基础设施，不声明业务模块 typed config。
- typed config 放在拥有该配置的模块内；例如 `AFS_ModelConfig` 位于 `models/`。
- 业务模块可以依赖 `AFS_ConfigManager` 单例读取 JSON、注册 schema，并在需要时注册应用回调。
- `config/` 不反向依赖 `models/`、`tui/` 或 `plugins/`。

## 注意事项

- `AFS_ConfigManager::loadFromFile()` 启动时读取配置文件到 `AFS_Config`，只校验文件可读、JSON 合法且根节点为 object。
- 必填字段、默认值和业务语义校验由模块 typed loader 或应用回调负责。
- 模型工厂 `createModel` 通过 `base_url` 字符串匹配识别协议，新增协议时在 models 模块扩展。
- 当前版本配置文件仅支持 JSON 格式，不支持 YAML/TOML。
