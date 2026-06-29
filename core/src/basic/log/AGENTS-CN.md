# core > src > basic > log > AGENTS-CN.md

全局日志与发布-订阅总线，基于 `spdlog` 日志库。程序启动时最先初始化。

## 文件

| 文件 | 职责 |
|------|------|
| `logger.hh` | `AFS_Logger` 单例 + 日志宏 |
| `logger.cc` | spdlog 初始化、格式化输出

## `AFS_Logger`

全局单例，程序唯一实例。两个日志通道 + 发布-订阅总线：
- `afs` — 系统日志（`[HH:MM:SS] [LEVEL] %v`）
- `afs_output` — 用户可见输出（`%v`，无前缀）
- 事件缓冲区 — Agent 运行时事件（Loop 发布 push，前端 poll 取出）

| 方法 | 说明 |
|------|------|
| `init()` | 初始化（程序启动时调用） |
| `instance()` | 获取单例 |
| `log(lvl, file, line, agent, role, msg)` | 带文件位置的日志 |
| `log(lvl, agent, role, msg)` | 不带文件位置的日志 |
| `output(msg)` | 用户可见输出 |
| `publishStart()` | 写入 Start 事件到缓冲区 |
| `publishAssistantMessage(msg_print)` | 写入 AssistantMessage 事件 |
| `publishToolResult(msg_print)` | 写入 ToolResult 事件 |
| `publishError(err)` | 写入 Error 事件 |
| `publishComplete(reply)` | 写入 Complete 事件 |
| `poll()` | 取出并清空全部缓冲事件 |

## 事件轮询

Loop 通过 `publish*()` 将事件写入 Logger 内部缓冲区，前端通过 `poll()` 取出：

```cpp
agent.run();

// 轮询事件缓冲区
auto events = AFS_Logger::instance().poll();
for (const auto& e : events) {
    switch (e.type) {
    case AgentEvent::AssistantMessage:
    case AgentEvent::ToolResult:
        if (e.message_print) AFS_Logger::instance().output(*e.message_print);
        break;
    case AgentEvent::Complete:
        if (!e.text.empty()) AFS_Logger::instance().output("\n" + e.text);
        break;
    // ...
    }
}
```

`poll()` 取出后清空缓冲区，无事件时返回空 vector。
控制台启动后台线程跑 `run()`，主线程 50ms 间隔 poll 实时渲染，TUI/GUI 类似。

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
