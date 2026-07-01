# core > src > basic > config > AGENTS-CN.md

动态配置基础设施。该目录维护通用 `AFS_Config` 配置类，负责 JSON 文件解析、配置路径、schema 注册表、模块应用回调和通用写回；不得声明或解析任何业务模块的具体配置结构。

## 文件

- `config.hh` — `AFS_Config`、`AFS_ConfigManager`、`AFS_ConfigSchema`、`AFS_ConfigFieldSpec`、`AFS_ConfigValueType` 等通用配置基础类型。
- `config.cc` — JSON 加载/保存、路径读取/写入、受影响模块计算、schema 注册去重、可选模块应用回调。
- `paths.hh` / `paths.cc` — 用户配置目录、默认配置文件、默认插件目录的路径解析。

## 边界

- `config/` **不知道** `models`、`tui`、`plugins` 的具体配置结构。
- 业务模块可以包含 `basic/config/config.hh`，注册自己的 `AFS_ConfigSchema`，并从 `AFS_Config` 解析自己的 typed config。
- `config/` 只有在模块调用 `registerSchema()` 后，才会在 schema 注册表中“看见”该模块；注册时可附带 `AFS_ConfigApplyFn`，用于配置写回后直接应用该模块。
- `AFS_ConfigManager::updateValue()` 修改 `AFS_Config` 中的 JSON 并计算受影响模块，不负责业务字段语义校验。字段语义由注册该 schema 的模块解析或应用时校验。

## 通用 schema 类型

### `AFS_ConfigValueType`

用于 TUI 等展示层选择输入校验方式：

- `String`
- `UnsignedInteger`
- `Number`
- `Boolean`

`AFS_ConfigValueTypeName(type)` 返回面向 UI 的字符串：`string`、`unsigned integer`、`number`、`boolean`。

### `AFS_ConfigFieldSpec`

| 字段 | 说明 |
|------|------|
| `name` | 字段名，相对于 schema path 的末段 |
| `type` | 输入类型 |
| `required` | 是否必填，由模块解析时决定如何执行 |
| `sensitive` | 是否敏感；TUI 可据此隐藏当前值 |
| `default_value` | UI 或模块可使用的默认值 |

### `AFS_ConfigSchema`

| 字段 | 说明 |
|------|------|
| `module` | 模块名，如 `models.llms`、`tui.layout`、`plugin.tool.example` |
| `path` | JSON 路径，如 `models.llms` 表示 `root["models"]["llms"]` |
| `is_array` | `true` 表示 path 指向数组，字段应用到数组每个元素 |
| `fields` | 该模块暴露给配置浏览/编辑界面的字段列表 |

## `AFS_Config`

配置值对象，保存已解析的 JSON 根和配置文件路径。

| 方法 | 说明 |
|------|------|
| `loadFromFile(path, error)` | 读取并解析 JSON 文件，要求根节点为 object |
| `save(error)` | 写回当前配置文件 |
| `path()` | 当前配置文件路径 |
| `root()` | 当前原始 JSON 根，只读 |
| `valueAt(path)` | 按路径读取 JSON 值，失败返回 `nullptr` |
| `updateValue(path, value, error)` | 按路径写入 JSON 值 |

## `AFS_ConfigManager`

进程级单例，负责持有当前 `AFS_Config`、模块 schema 和可选应用回调。

| 方法 | 说明 |
|------|------|
| `instance()` | 返回全局配置管理器 |
| `registerSchema(schema, apply)` | 注册模块 schema 和可选应用回调；同 `module + path` 重复注册时覆盖旧值 |
| `schemas()` | 返回当前已注册 schema 列表 |
| `loadFromFile(path, error)` | 启动时读取 JSON 到内部 `AFS_Config` |
| `save(error)` | 将内部 `AFS_Config` 写回已加载路径 |
| `config()` | 返回当前 `AFS_Config` |
| `path()` / `root()` / `valueAt(path)` | 只读代理到当前 `AFS_Config` |
| `updateValue(path, value)` | 按路径写入 JSON 值，并返回受影响模块 |
| `applyModule(module, error)` | 若模块注册过应用回调，则用当前 `AFS_Config` 调用该回调 |
| `applyModules(modules, error)` | 依次应用多个受影响模块 |

## 模块接入方式

以 models 模块为例：

1. 在 `basic/models/model.hh` 声明 `AFS_ModelConfig`、`AFS_ModelsConfig`。
2. 在 `basic/models/model.cc` 实现 `from_json` 和 `AFS_ModelConfig::configSchema()`。
3. 模块启动时调用 `AFS_RegisterModelConfigSchemas()`，向 `AFS_ConfigManager` 注册 `models.llms` 和 `models.embeddings`。
4. 加载配置文件后，调用 `AFS_LoadModelsConfig(AFS_ConfigManager::instance().config())` 从 `AFS_Config` 解析模型配置。

TUI 模块同理：`AFS_TuiLayoutConfig` / `AFS_TuiConfig` 在 `tui/app.hh` 中声明，`AFS_TuiApp::registerConfigSchema()` 注册 `tui.layout`。

插件模块通过可选 ABI `pluginConfigSchemas()` 返回 JSON schema 数组，`AFS_PluginManager` 加载插件时注册这些 schema。

## JSON 文件约定

当前默认配置文件仍使用以下形状，但 `config/` 不硬编码这些字段：

```json
{
  "models": {
    "llms": [
      {
        "name": "DeepSeek",
        "base_url": "https://api.deepseek.com",
        "api_key": "sk-...",
        "model": "deepseek-chat",
        "context_limit": 1000000
      }
    ],
    "embeddings": []
  },
  "tui": {
    "layout": {
      "sidebar_ratio": 0.35
    }
  }
}
```

## 设计决策

- **动态边界**：配置核心只维护 JSON 与 schema 注册表，模块配置类型留在模块目录内。
- **注册后可见**：TUI 配置页只枚举已注册 schema；模块未注册时不会出现在配置浏览器中。
- **按模块重载**：写入后通过 `affected_modules` 判断是否需要重载模型、TUI 布局或插件。
- **无业务默认值注入**：`config/` 不替模块补业务默认值；模块 typed loader 负责默认值和校验。

## 依赖

- `nlohmann_json` — JSON 解析与序列化。

## 注意事项

- 不要在 `config/` 中新增 `AFS_ModelsConfig`、`AFS_TuiConfig` 等模块结构。
- 新模块需要配置时，应在该模块头/源文件中声明 typed config，并提供注册函数。
- 修改配置保存行为时，应保持 `updateValue()` 对未知 path 的通用写入能力。
