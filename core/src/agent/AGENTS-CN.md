# core > src > agent > AGENTS-CN.md

Agent 核心定义及子结构。

## 文件

| 文件 | 职责 |
|------|------|
| `agent.hh` | `AFS_Agent` 类声明 |
| `agent.cc` | `AFS_Agent` 实现 |
| `loop/` | Agent 运行核心循环（待实现） |

## 类定义 `AFS_Agent`

Agent 核心节点，程序中的 Agent 之间为树状结构。

### 规则

1. 一个程序有且只有一个主 Agent（`level == 0`）。
2. 父节点通过 `std::unique_ptr` 独占子节点所有权，只有父节点有权操作子节点。
3. `genSubNode()` 返回 `AFS_Agent&` 引用（非拥有），引用在父节点存活期间有效。
4. 删除节点时，`unique_ptr` 析构自动递归销毁所有后代节点。
5. 子 Agent 继承父 Agent 的已注册工具（`tool_registry_` 拷贝）。
6. 所有 Agent 构造时自动初始化默认上下文（系统提示词），仅由 Agent 自身管理。
7. 主 Agent（level==0）构造时自动调用 `registerTools()` 注册所有已加载工具插件。

### 接口

| 方法 | 返回 | 说明 |
|------|------|------|
| `createMain()` (static) | `std::unique_ptr<AFS_Agent>` | 创建主 Agent，全局仅允许一次，重复返回 `nullptr` |
| `genSubNode()` | `AFS_Agent&` | 生成子节点（level + 1），返回非拥有引用 |
| `killSubNode(AFS_Agent&)` | `void` | 杀死直接子节点及所有后代 |
| `swapWithChild(self, child)` (static) | `std::unique_ptr<AFS_Agent>` | 将 child 提升到 self 的位置，原 self 成为 child 的子节点。用法: `ptr = AFS_Agent::swapWithChild(std::move(ptr), child)` |
| `level()` | `unsigned` | 节点层级，0 为主 Agent |
| `isMain()` | `bool` | 是否为主 Agent |
| `subCount()` | `size_t` | 直接子节点数量 |
| `uuid()` | `const std::string&` | 8 位十六进制唯一标识符 |
| `registerTools()` | `void` | 从插件管理器注册所有已加载工具插件（主 Agent 构造时自动调用） |
| `loadExtraTool(type, name)` | `void` | 加载指定插件并注册其工具，并在上下文追加工具提示 |
| `removeTool(name)` | `void` | 移除已注册的工具 |
| `toolRegistry()` | `const AFS_ToolRegistry&` | 获取工具注册表 |
| `context()` | `AFS_Context&` | 获取上下文管理器 |
| `printMain(root)` (static) | `std::string` | 递归打印以 root 为根的 Agent 树 |

### `printMain` 输出格式

```
[level=0, sub=2, uuid=00000000]
+-- [level=1, sub=1, uuid=00000001]
|   +-- [level=2, sub=0, uuid=00000003]
+-- [level=1, sub=0, uuid=00000002]
```

每行以 `+--` 开头表示子节点，`|` 竖线连接非末位兄弟节点的后代，末位兄弟节点用 4 空格延续。

### `swapWithChild` 语义

```
交换前:                    交换后:
  self (level=0)            promoted (level=0)
  ├── child   ← promoted    ├── self     (level=1, 原 self)
  ├── sibling1              ├── sibling1 (level=1)
  └── sibling2              ├── sibling2 (level=1)
                            └── child's old children (层级自动修正)
```

采用 `static` 方法接收 `unique_ptr<AFS_Agent>`（移动语义），完成所有权转移后返回新的根节点。子树层级自动递归修正。
同时交换 `tool_registry_` 和 `loaded_plugins_`，确保提升后的节点持有原父级的工具注册表和插件引用。

### 私有成员

| 字段 | 类型 | 说明 |
|------|------|------|
| `level_` | `unsigned` | Agent 层级，0 = 主 Agent |
| `uuid_` | `std::string` | 8 位十六进制 UUID，构造时通过 `generateUuid8()` 自增生成 |
| `sub_agent_nodes_` | `std::vector<std::unique_ptr<AFS_Agent>>` | 子节点列表，独占所有权 |
| `tool_registry_` | `AFS_ToolRegistry` | 工具注册表，子 Agent 构造时拷贝父级 |
| `context_` | `AFS_Context` | 上下文管理器，构造时自动初始化默认系统提示词 |
| `loaded_plugins_` | `std::vector<std::pair<AFS::PluginType,std::string>>` | 本 Agent 加载的插件（析构时释放引用） |

### 私有方法

| 方法 | 说明 |
|------|------|
| `fixSubtreeLevels(node)` (static) | 递归修正 node 子树中所有节点的层级 |
| `printNode(out, node, indent)` (static) | 递归辅助 `printMain` |
| `initDefaultContext()` | 构造时初始化默认上下文 |

### 全局计数器

`createMain()` 通过文件作用域 `g_main_created` 布尔标志确保主 Agent 全局唯一。

## 所有权模型

```
main_agent (unique_ptr, 外部持有)
  ├── child_a (unique_ptr, main_agent 持有) ← genSubNode() 返回引用
  │     └── grandchild (unique_ptr, child_a 持有)
  └── child_b (unique_ptr, main_agent 持有)
```

- 父节点通过 `std::vector<std::unique_ptr<AFS_Agent>>` 独占子节点所有权。
- `genSubNode()` 将新节点的 `unique_ptr` 存入向量，同时返回 `AFS_Agent&` 引用。
- `killSubNode(AFS_Agent&)` 通过地址匹配从向量中移除对应的 `unique_ptr`，析构自动级联清理。
- `swapWithChild` 通过移动语义安全转移所有权，`fixSubtreeLevels` 保证层级一致性。
- 析构时遍历 `loaded_plugins_`，对每个插件调用 `pm->unloadPlugin()` 释放引用计数。
- 子 Agent 通过拷贝构造继承父级的 `tool_registry_`，但不继承 `loaded_plugins_`。
- 子 Agent 的 `context_` 初始为空，由父 Agent 根据子任务需要显式构造初始上下文。

## 生命周期与销毁顺序

`AFS_Agent` 的析构必须保证严格的资源释放顺序，否则会导致 SIGSEGV：

```
~AFS_Agent()
  1. sub_agent_nodes_.clear()
     └── 递归销毁所有子 Agent
         ├── 子 Agent 的 sub_agent_nodes_.clear()
         ├── 子 Agent 的 tool_registry_ = {}   ← 释放函数对象（.so 仍加载）
         └── 子 Agent 遍历 loaded_plugins_      ← 通常为空
  2. tool_registry_ = AFS_ToolRegistry{}         ← 释放本节点函数对象
  3. 遍历 loaded_plugins_ → pm->unloadPlugin()  ← ref-1, ref=0 则 dlclose
```

**关键约束**：`tool_registry_` 中存储的 `std::function` 持有插件 `.so` 中的函数指针。
必须在 `dlclose` 之前释放这些函数对象，否则析构时访问野指针导致崩溃。

**继承关系**：
- 子 Agent 通过拷贝获得父级的 `tool_registry_`（包含相同的函数对象引用）
- 只有实际调用 `registerTools()` 或 `loadExtraTool()` 的 Agent 持有 `loaded_plugins_` 记录
- 销毁时由持有记录的 Agent 负责释放插件引用，其他 Agent 只清理自己的 `tool_registry_`

## 子目录

| 目录 | 职责 |
|------|------|
| `context/` | 上下文管理器（`AFS_Context`） |
| `loop/` | Agent 核心循环（`AFS_Loop`），基于 boost::sml 状态机 |
| `tool/` | 工具调用数据结构（`AFS_ToolSpec`、`AFS_ToolCall`、`AFS_ToolResult`） |

## 约定

- `AFS_Agent` 禁止拷贝和赋值（`= delete`）。
- 构造函数私有，只能通过 `createMain()` 或 `genSubNode()` 创建实例。
- `genSubNode()` 返回的引用与父节点生命周期绑定，父节点销毁后引用失效。
- 后续 `AFS_Loop` 将作为 `AFS_Agent` 的成员，驱动单个 Agent 的事件循环。
