# plugins > tools > bash > AGENTS-CN.md

Shell 命令执行工具插件，通过 `AFS::Plugin::toolCapabilities()` 暴露。

## 文件

| 文件 | 职责 |
|------|------|
| `bash.cpp` | 插件源码（JSON 解析、进程执行、结果序列化） |
| `build.sh` | 编译/安装/清理脚本 |

## 功能

通过 `/bin/bash -lc` 执行 Shell 命令，捕获合并后的 stdout+stderr，返回输出内容和退出码。

**输入格式**（JSON 字符串）：
```json
{"command": "echo hello && ls /tmp"}
```

**输出格式**：
```json
{"output": "hello\nfile1\nfile2\n", "exit_code": 0}
```

| 场景 | command | 输出示例 |
|------|---------|---------|
| 正常执行 | `echo ok` | `{"output":"ok\n","exit_code":0}` |
| 命令失败 | `false` | `{"output":"","exit_code":1}` |
| 命令不存在 | `nonexistent_cmd` | `{"output":"bash: line 1: nonexistent_cmd: command not found\n","exit_code":127}` |
| 空命令 | `""` | `{"error":"command is empty"}` |
| 被信号终止 | `kill $$` | `{"output":"","exit_code":143}` |

## 代码结构

```cpp
#include <afs.hh>          // 公共 API 头文件

// 内部实现
ExecResult execCommand(const std::string& command) {
    // pipe → fork → dup2(stdout/stderr) → execl("/bin/bash", "bash", "-lc", command)
    // 父进程读取输出并 waitpid 提取退出码
}

class BashPlugin final : public AFS::Plugin {
    // name(), type(), start(), stop()

    std::vector<ToolCap> toolCapabilities() const override {
        return {
            {"bash",
             "Execute a shell command via /bin/bash...",
             R"({...json schema...})",
             [](const std::string& input) -> std::string {
                 // 解析 command → execCommand → 返回 JSON
             }},
        };
    }
};

// C ABI 导出
AFS_PLUGIN_EXPORT uint32_t pluginAbiVersion() { ... }
AFS_PLUGIN_EXPORT AFS::Plugin* createPlugin() { ... }
AFS_PLUGIN_EXPORT void destroyPlugin(AFS::Plugin* p) { ... }
```

## 实现要点

- 使用 `pipe` + `fork` + `dup2` 合并 stdout 和 stderr，避免命令字符串二次转义
- 子进程使用 `execl("/bin/bash", "bash", "-lc", command, nullptr)` 执行命令
- 父进程通过 `waitpid` 获取退出状态，处理正常退出、信号终止两种情况
- 信号终止时退出码 = `128 + signo`，与 shell 惯例一致
- JSON 输入支持常见字符串转义（`\"`, `\\`, `\n`, `\r`, `\t` 等）
- JSON 输出中的特殊字符和控制字符会进行转义

## 编译

```sh
# 从顶层编译
cd plugins && ./build.sh bash && ./build.sh bash install

# 或单独编译
cd plugins/tools/bash
./build.sh          # → ToolPluginBash
./build.sh install  # → 安装到 bin/plugins/tool/，并删除本目录临时二进制
./build.sh clean    # 清理
```

编译依赖：C++23 编译器，`core/include/` 公共头文件。JSON 解析内嵌轻量实现，无需外部库。

## 安装后验证

```sh
# 启动 Agent 后查看
--- 插件管理器 ---
已加载工具插件: 1 个
  [ToolSpec] bash: Execute a shell command via /bin/bash...

# 执行工具
[ToolCall ...] bash({"command":"echo hello"})
[ToolResult ...] bash OK: {"output":"hello\n","exit_code":0}
```
