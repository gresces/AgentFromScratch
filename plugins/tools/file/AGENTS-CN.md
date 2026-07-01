# plugins > tools > file > AGENTS-CN.md

文件读写工具插件，通过 `AFS::Plugin::toolCapabilities()` 暴露五个工具。
核心增强：**hashline 机制**——读取时每行带 6 位行号前缀，编辑时基于行号精准替换。

## 文件

| 文件 | 职责 |
|------|------|
| `file.cpp` | 插件源码（JSON 解析、hashline 渲染、行级编辑、文件 I/O） |
| `build.sh` | 编译/安装/清理脚本 |

## 功能

提供文件存在性检查、hashline 读取、全量写入、行级编辑和追加写入五个子工具。

### 工具列表

| 工具名 | 功能 | 输入参数 |
|--------|------|---------|
| `file_read` | hashline 格式读取（最大 1 MiB） | `path`, `offset?`, `limit?` |
| `file_write` | 全量覆盖写入 | `path`, `content` |
| `file_exists` | 检查文件是否存在 | `path` |
| `file_edit` | 基于行号的精确替换 | `path`, `start_line`, `end_line`, `content` |
| `file_append` | 追加写入（文件不存在则创建） | `path`, `content` |

## Hashline 机制

### 读取格式

`file_read` 返回的内容每行带有 6 位数字前缀：

```
000001|#include <stdio.h>
000002|
000003|int main() {
000004|    printf("hello\n");
000005|    return 0;
000006|}
```

### 编辑流程

1. 用 `file_read` 查看文件，获取带行号的内容
2. 根据行号确定要修改的范围
3. 用 `file_edit` 指定 `start_line` / `end_line`（1-based, inclusive）和新的 `content`
4. 可选再用 `file_read` 验证修改结果

典型示例：将第 4 行的 `"hello"` 改为 `"hello world"`
```json
{"path": "/tmp/test.c", "start_line": 4, "end_line": 4,
 "content": "    printf(\"hello world\\n\");\n"}
```

### 行号语义

- 所有行号均为 **1-based**
- `start_line` 和 `end_line` 均为 **inclusive**（含两端）
- 若 `start_line` 超出文件行数，会在末尾填充空行后插入
- `end_line` 超出文件行数时，替换到文件末尾

## 输入格式

```json
// file_read（offset/limit 可选）
{"path": "/tmp/test.txt"}
{"path": "/tmp/test.txt", "offset": 100, "limit": 50}

// file_write
{"path": "/tmp/test.txt", "content": "hello world\n"}

// file_exists
{"path": "/tmp/test.txt"}

// file_edit
{"path": "/tmp/test.txt", "start_line": 3, "end_line": 5,
 "content": "new line 3\nnew line 4\n"}

// file_append
{"path": "/tmp/test.txt", "content": "appended line\n"}
```

## 输出格式

| 场景 | 输出示例 |
|------|---------|
| 读取成功 | `{"content":"000001|hello\n000002|world\n","start_line":1,"end_line":2,"total_lines":2}` |
| 读取范围 | `{"content":"000100|...\n","start_line":100,"end_line":149,"total_lines":500}` |
| 读取超出范围 | `{"content":"","start_line":999,"end_line":998,"total_lines":10}` |
| 读取失败（不存在） | `{"error":"open \"/tmp/missing.txt\": No such file or directory"}` |
| 读取失败（文件过大） | `{"error":"file too large (max 1 MiB)"}` |
| 写入成功 | `{"written":12}` |
| 写入失败（无权限） | `{"error":"open \"/root/secret\": Permission denied"}` |
| 文件存在 | `{"exists":true}` |
| 文件不存在 | `{"exists":false}` |
| 编辑成功 | `{"replaced_lines":3,"inserted_lines":2,"total_lines":49}` |
| 编辑失败（参数错误） | `{"error":"start_line must be >= 1"}` |
| 追加成功 | `{"appended":14}` |
| 缺少参数 | `{"error":"path is required and must be a string"}` |

## 代码结构

```cpp
#include <afs.hh>          // 公共 API 头文件

// ---- 内部实现 ----

// 行拆分：将原始内容按 \n 拆分为带换行符的行数组
std::vector<std::string> splitLines(const std::string& content);

// Hashline 渲染：将行数组格式化为 "000001|content\n" 形式
std::string renderHashline(const std::vector<std::string>& lines, int startLine);

// 各工具实现
std::string doFileRead(const std::string& path, int offset, int limit);
std::string doFileWrite(const std::string& path, const std::string& content);
std::string doFileExists(const std::string& path);
std::string doFileEdit(const std::string& path, int startLine, int endLine,
                       const std::string& newContent);
std::string doFileAppend(const std::string& path, const std::string& content);

class FilePlugin final : public AFS::Plugin {
    // name(), type(), start(), stop()

    std::vector<ToolCap> toolCapabilities() const override {
        return {
            {"file_read",   "...", "{...}", [](auto& in) { ... }},
            {"file_write",  "...", "{...}", [](auto& in) { ... }},
            {"file_exists", "...", "{...}", [](auto& in) { ... }},
            {"file_edit",   "...", "{...}", [](auto& in) { ... }},
            {"file_append", "...", "{...}", [](auto& in) { ... }},
        };
    }
};

// C ABI 导出
AFS_PLUGIN_EXPORT uint32_t pluginAbiVersion() { ... }
AFS_PLUGIN_EXPORT AFS::Plugin* createPlugin() { ... }
AFS_PLUGIN_EXPORT void destroyPlugin(AFS::Plugin* p) { ... }
```

## 实现要点

- **Hashline 行号**：6 位零填充（`%06d`），覆盖最多 999999 行
- **读取上限** `MaxReadSize = 1 MiB`，防止大文件撑爆内存
- **默认 limit**：`DefaultLimit = 2000` 行，避免单次返回过多内容
- **行拆分**保留 `\n` 换行符，确保编辑后换行不丢失
- **file_edit** 支持超出文件范围的行号：start_line 超出时自动填充空行
- 文件 I/O 使用二进制模式，避免平台差异
- JSON 输入支持常见字符串转义
- 错误消息包含操作类型、路径和 `strerror(errno)` 信息

## 编译

```sh
cd plugins && ./build.sh file && ./build.sh file install

# 或单独编译
cd plugins/tools/file
./build.sh          # → ToolPluginFile
./build.sh install  # → 安装并清理本地产物
./build.sh clean    # 清理
```

编译依赖：C++23 编译器，`core/include/` 公共头文件。JSON 解析和文件 I/O 内嵌实现，无需外部库。

## 安装后验证

```sh
# 启动 Agent 后查看
--- 插件管理器 ---
已加载工具插件: 1 个
  [ToolSpec] file_read: Read a file ...
  [ToolSpec] file_write: Write content ...
  [ToolSpec] file_exists: Check whether ...
  [ToolSpec] file_edit: Replace a line range ...
  [ToolSpec] file_append: Append content ...

# Hashline 读取
[ToolCall] file_read({"path":"/tmp/test.txt"})
[ToolResult] file_read OK:
{
  "content": "000001|line one\n000002|line two\n000003|line three\n",
  "start_line": 1,
  "end_line": 3,
  "total_lines": 3
}

# 精确编辑（替换第2行）
[ToolCall] file_edit({"path":"/tmp/test.txt","start_line":2,"end_line":2,
                       "content":"line two modified\n"})
[ToolResult] file_edit OK:
{"replaced_lines":1,"inserted_lines":1,"total_lines":3}

# 追加
[ToolCall] file_append({"path":"/tmp/test.txt","content":"line four\n"})
[ToolResult] file_append OK: {"appended":10}
```
