# plugins > context > simple > AGENTS-CN.md

Context 运行时插件，提供 Agent 默认上下文实现。

## 文件

| 文件 | 职责 |
|------|------|
| `context.cpp` | `AFS::Context` 默认实现与 `ContextPluginSimple` 插件入口 |
| `xmake.lua` | 插件 xmake 构建定义 |
| `build.sh` | 兼容顶层 `plugins/build.sh` 的构建/安装/清理包装脚本 |

## 功能

- 保存 `AFS::Message` 消息历史。
- 提供 `addMessage()`、`messages()`、`messageCount()`、`buildRequest()`、`buildPrompt()`、`clear()`。
- 通过宿主注入的 token 计数回调维护 `tokenCount()`。

## 安装路径

构建产物为 `ContextPluginSimple`，安装到：

```sh
${XDG_CONFIG_HOME:-~/.config}/afs/plugins/context/ContextPluginSimple
```
