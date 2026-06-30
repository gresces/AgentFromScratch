# plugins > tools > file > AGENTS-CN.md

文件读写工具插件，通过 `AFS::Plugin::toolCapabilities()` 暴露三个工具。

## 文件

| 文件 | 职责 |
|------|------|
| `file.cpp` | 插件源码（JSON 解析、文件 I/O、结果序列化） |
| `build.sh` | 编译/安装/清理脚本 |

## 功能

提供文件存在性检查、读取和写入三个子工具，使用标准 C++ 文件流实现，无需外部依赖。

### 工具列表

| 工具名 | 功能 | 输入参数 |
|--------|------|---------|
| `file_read` | 读取文件内容（最大 1 MiB） | `path` |
| `file_write` | 写入内容到文件（覆盖） | `path`, `content` |
| `file_exists` | 检查文件是否存在 | `path` |

### 输入格式

```json
{"path": "/tmp/test.txt"}
{"path": "/tmp/test.txt", "content": "hello world"}
```

### 输出格式

| 场景 | 输出示例 |
|------|---------|
| 读取成功 | `{"content":"hello world\n","size":12}` |
| 读取失败（不存在） | `{"error":"open \"/tmp/missing.txt\": No such file or directory"}` |
| 读取失败（文件过大） | `{"error":"file too large (max 1 MiB)"}` |
| 写入成功 | `{"written":12}` |
| 写入失败（无权限） | `{"error":"open \"/root/secret\": Permission denied"}` |
| 文件存在 | `{"exists":true}` |
| 文件不存在 | `{"exists":false}` |
| 缺少参数 | `{"error":"path is required and must be a string"}` |
| 空路径 | `{"error":"path is empty"}` |

## 代码结构

```cpp
#include <afs.hh>          // 公共 API 头文件

// 内部实现
std::string readFile(const std::string& path) {
    // std::ifstream 二进制模式 → 检查大小上限 → 读取 → JSON 序列化
}
std::string writeFile(const std::string& path, const std::string& content) {
    // std::ofstream 二进制截断模式 → 写入 → 返回字节数
}
std::string checkExists(const std::string& path) {
    // stat() 检查是否为常规文件
}

class FilePlugin final : public AFS::Plugin {
    // name(), type(), start(), stop()

    std::vector<ToolCap> toolCapabilities() const override {
        return {
            {"file_read",  "Read a file ...", "{...}", [](auto& in) { ... }},
            {"file_write", "Write a file ...","{...}", [](auto& in) { ... }},
            {"file_exists","Check file ...", "{...}", [](auto& in) { ... }},
        };
    }
};

// C ABI 导出
AFS_PLUGIN_EXPORT uint32_t pluginAbiVersion() { ... }
AFS_PLUGIN_EXPORT AFS::Plugin* createPlugin() { ... }
AFS_PLUGIN_EXPORT void destroyPlugin(AFS::Plugin* p) { ... }
```

## 实现要点

- 读取上限 `MaxReadSize = 1 MiB`，防止大文件撑爆内存
- 文件 I/O 使用 `std::ifstream` / `std::ofstream` 二进制模式，避免文本模式下的平台差异（如 Windows `\r\n` 转换）
- 使用 `stat()` 检查文件存在性，`S_ISREG` 过滤目录、设备等非普通文件
- `file_write` 使用 `std::ios::trunc` 模式，每次写入覆盖已有内容
- JSON 输入支持常见字符串转义（`\"`, `\\`, `\n`, `\r`, `\t` 及 `\uXXXX`）
- JSON 输出中的特殊字符和控制字符会进行转义
- 错误消息包含操作类型、路径和 `strerror(errno)` 信息，便于定位问题

## 编译

```sh
# 从顶层编译
cd plugins && ./build.sh file && ./build.sh file install

# 或单独编译
cd plugins/tools/file
./build.sh          # → ToolPluginFile
./build.sh install  # → 安装到 ${XDG_CONFIG_HOME:-~/.config}/afs/plugins/tool/，并删除本地临时产物
./build.sh clean    # 清理
```

编译依赖：C++23 编译器，`core/include/` 公共头文件。JSON 解析和文件 I/O 内嵌实现，无需外部库。

## 安装后验证

```sh
# 启动 Agent 后查看
--- 插件管理器 ---
已加载工具插件: 1 个
  [ToolSpec] file_read: Read a file from disk. ...
  [ToolSpec] file_write: Write content to a file. ...
  [ToolSpec] file_exists: Check whether a file exists. ...

# 执行工具
[ToolCall ...] file_exists({"path":"/tmp/test.txt"})
[ToolResult ...] file_exists OK: {"exists":false}

[ToolCall ...] file_write({"path":"/tmp/test.txt","content":"hello"})
[ToolResult ...] file_write OK: {"written":5}

[ToolCall ...] file_read({"path":"/tmp/test.txt"})
[ToolResult ...] file_read OK: {"content":"hello","size":5}
```
