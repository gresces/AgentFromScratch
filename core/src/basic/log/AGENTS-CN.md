# core > src > basic > log > AGENTS-CN.md

全局日志模块，基于 `spdlog` 日志库。程序启动时最先初始化。

## 文件

| 文件 | 职责 |
|------|------|
| `logger.hh` | `AFS_Logger` 单例 + 日志宏 |
| `logger.cc` | spdlog 初始化、格式化输出

## `AFS_Logger`

全局单例，程序唯一实例。两个日志通道：
- `afs` — 系统日志（`[HH:MM:SS] [LEVEL] %v`）
- `afs_output` — 用户可见输出（`%v`，无前缀）

| 方法 | 说明 |
|------|------|
| `init()` | 初始化（程序启动时调用） |
| `instance()` | 获取单例 |
| `log(lvl, file, line, agent, role, msg)` | 带文件位置的日志 |
| `log(lvl, agent, role, msg)` | 不带文件位置的日志 |
| `output(msg)` | 用户可见输出 |

## 宏

| 宏 | 说明 |
|------|------|
| `AFS_LOG_ERROR(agent, role, msg)` | 错误日志（自动捕获 `__FILE__` + `__LINE__`） |
| `AFS_LOG_WARN(agent, role, msg)` | 警告日志 |
| `AFS_LOG_INFO(agent, role, msg)` | 信息日志 |

## 角色标签

| 常量 | 含义 |
|------|------|
| `kRoleSystem` | 系统消息 |
| `kRoleUser` | 用户消息 |
| `kRoleAssistant` | 助手消息 |
| `kRoleTool` | 工具调用 |
| `kRoleLoop` | 循环事件 |
| `kRolePlugin` | 插件事件 |
| `kRoleMain` | 主程序 |

## 日志格式

```
HH:MM:SS [LOG_LEVEL] [agent_uuid][ROLE] filename:line | message

例:
13:31:14 [ERROR] [00000000][LOOP] loop.cc:142 | LLM 请求失败
```

## 使用

```cpp
#include "basic/log/logger.hh"

int main() {
    AFS_Logger::init();  // 最先初始化

    AFS_LOG_ERROR("main", kRoleLoop, "连接失败");
    // → 14:22:05 [ERROR] [main][LOOP] main.cc:10 | 连接失败

    AFS_Logger::instance().output("Hello World");
    // → Hello World
}
```

## 依赖

- `spdlog`（日志库）
