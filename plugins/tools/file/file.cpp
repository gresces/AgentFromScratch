#include <afs.hh>

#include <cerrno>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <sstream>
#include <string>
#include <sys/stat.h>
#include <vector>

namespace {

// ---- constants --------------------------------------------------------------

constexpr size_t MaxReadSize = 1024 * 1024; // 1 MiB
constexpr int    DefaultLimit = 2000;        // 默认返回行数上限

// ---- json helpers -----------------------------------------------------------

size_t skipJsonWhitespace(const std::string& json, size_t pos) {
    while (pos < json.size()) {
        char c = json[pos];
        if (c != ' ' && c != '\t' && c != '\n' && c != '\r') return pos;
        ++pos;
    }
    return pos;
}

bool appendJsonEscape(std::string& value, char escape) {
    switch (escape) {
    case '"':  value += '"';  return true;
    case '\\': value += '\\'; return true;
    case '/':  value += '/';  return true;
    case 'b':  value += '\b'; return true;
    case 'f':  value += '\f'; return true;
    case 'n':  value += '\n'; return true;
    case 'r':  value += '\r'; return true;
    case 't':  value += '\t'; return true;
    default:   return false;
    }
}

bool extractString(const std::string& json, const std::string& key, std::string& value) {
    const std::string quoted_key = "\"" + key + "\"";
    size_t pos = json.find(quoted_key);
    if (pos == std::string::npos) return false;

    pos = skipJsonWhitespace(json, pos + quoted_key.size());
    if (pos >= json.size() || json[pos] != ':') return false;

    pos = skipJsonWhitespace(json, pos + 1);
    if (pos >= json.size() || json[pos] != '"') return false;

    value.clear();
    ++pos;
    while (pos < json.size()) {
        char c = json[pos++];
        if (c == '"') return true;
        if (c != '\\') {
            value += c;
            continue;
        }
        if (pos >= json.size()) return false;
        char escape = json[pos++];
        if (escape == 'u') {
            if (pos + 4 > json.size()) return false;
            value += "\\u";
            value.append(json, pos, 4);
            pos += 4;
            continue;
        }
        if (!appendJsonEscape(value, escape)) return false;
    }
    return false;
}

// 从 JSON 中提取整数值，key 不存在时返回 defaultValue
int extractInt(const std::string& json, const std::string& key, int defaultValue) {
    std::string search = "\"" + key + "\":";
    size_t pos = json.find(search);
    if (pos == std::string::npos) return defaultValue;
    pos += search.size();
    pos = skipJsonWhitespace(json, pos);
    if (pos >= json.size()) return defaultValue;

    // 支持负数
    bool neg = false;
    if (json[pos] == '-') { neg = true; ++pos; }

    int val = 0;
    bool found = false;
    while (pos < json.size() && json[pos] >= '0' && json[pos] <= '9') {
        val = val * 10 + (json[pos] - '0');
        ++pos;
        found = true;
    }
    if (!found) return defaultValue;
    return neg ? -val : val;
}

void appendJsonUnicodeEscape(std::string& output, unsigned char c) {
    char escaped[7] = {};
    std::snprintf(escaped, sizeof(escaped), "\\u%04x", c);
    output += escaped;
}

std::string jsonEscape(const std::string& value) {
    std::string output;
    output.reserve(value.size());
    for (unsigned char c : value) {
        switch (c) {
        case '"':  output += "\\\""; break;
        case '\\': output += "\\\\"; break;
        case '\b': output += "\\b";  break;
        case '\f': output += "\\f";  break;
        case '\n': output += "\\n";  break;
        case '\r': output += "\\r";  break;
        case '\t': output += "\\t";  break;
        default:
            if (c < 0x20) {
                appendJsonUnicodeEscape(output, c);
            } else {
                output += static_cast<char>(c);
            }
            break;
        }
    }
    return output;
}

// ---- file helpers -----------------------------------------------------------

std::string errnoMessage(const char* operation, const std::string& path) {
    return std::string(operation) + " \"" + path + "\": " + std::strerror(errno);
}

bool fileExists(const std::string& path) {
    struct stat st;
    return stat(path.c_str(), &st) == 0 && S_ISREG(st.st_mode);
}

// 将文件内容按行拆分（保留行尾换行符）
std::vector<std::string> splitLines(const std::string& content) {
    std::vector<std::string> lines;
    size_t start = 0;
    while (start < content.size()) {
        size_t end = content.find('\n', start);
        if (end == std::string::npos) {
            // 最后一行（可能没有换行符）
            lines.push_back(content.substr(start));
            break;
        }
        // 包含换行符
        lines.push_back(content.substr(start, end - start + 1));
        start = end + 1;
    }
    return lines;
}

// 读取整个文件原始内容；出错返回空 optional
std::pair<std::string, std::string> readFileRaw(const std::string& path) {
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file) {
        return {"", errnoMessage("open", path)};
    }

    std::streamsize size = file.tellg();
    if (size < 0) {
        return {"", "failed to determine file size"};
    }
    if (static_cast<size_t>(size) > MaxReadSize) {
        return {"", "file too large (max 1 MiB)"};
    }

    file.seekg(0, std::ios::beg);
    std::string content(static_cast<size_t>(size), '\0');
    if (!file.read(content.data(), size)) {
        return {"", errnoMessage("read", path)};
    }

    return {content, ""};
}

// 将行数组写入文件
std::string writeLines(const std::string& path, const std::vector<std::string>& lines) {
    std::ofstream file(path, std::ios::binary | std::ios::trunc);
    if (!file) {
        return errnoMessage("open", path);
    }
    for (const auto& line : lines) {
        file.write(line.data(), static_cast<std::streamsize>(line.size()));
        if (!file) {
            return errnoMessage("write", path);
        }
    }
    return "";
}

// ---- hashline ---------------------------------------------------------------
// 将行数组渲染为 hashline 格式字符串:
//   000001|content of line 1
//   000002|content of line 2
//   ...
// startLine: 起始行号（1-based）
std::string renderHashline(const std::vector<std::string>& lines,
                           int startLine) {
    std::string out;
    out.reserve(lines.size() * 80); // 粗略预估
    for (size_t i = 0; i < lines.size(); ++i) {
        char prefix[10];
        std::snprintf(prefix, sizeof(prefix), "%06d|", startLine + static_cast<int>(i));
        out += prefix;
        out += lines[i];
        // 如果最后一行不以 \n 结尾，补一个换行
        if (i == lines.size() - 1 && !lines[i].empty() && lines[i].back() != '\n') {
            out += '\n';
        }
    }
    return out;
}

// ---- tool implementations ---------------------------------------------------

// file_read: 带 hashline 的行读取
std::string doFileRead(const std::string& path, int offset, int limit) {
    auto [raw, error] = readFileRaw(path);
    if (!error.empty()) {
        return "{\"error\":\"" + jsonEscape(error) + "\"}";
    }

    auto allLines = splitLines(raw);
    int totalLines = static_cast<int>(allLines.size());

    // 参数校验 & 钳位
    if (offset < 1) offset = 1;
    if (limit < 1) limit = DefaultLimit;

    int startIdx = offset - 1; // 转为 0-based
    if (startIdx >= totalLines) {
        // 超出范围：返回空内容
        return "{\"content\":\"\",\"start_line\":" + std::to_string(offset) +
               ",\"end_line\":" + std::to_string(offset - 1) +
               ",\"total_lines\":" + std::to_string(totalLines) + "}";
    }

    int endIdx = std::min(startIdx + limit, totalLines);
    std::vector<std::string> slice(allLines.begin() + startIdx,
                                   allLines.begin() + endIdx);

    std::string hashed = renderHashline(slice, offset);

    return "{\"content\":\"" + jsonEscape(hashed) + "\"," +
           "\"start_line\":" + std::to_string(offset) + "," +
           "\"end_line\":" + std::to_string(offset + static_cast<int>(slice.size()) - 1) + "," +
           "\"total_lines\":" + std::to_string(totalLines) + "}";
}

// file_write: 全量覆盖写入
std::string doFileWrite(const std::string& path, const std::string& content) {
    std::ofstream file(path, std::ios::binary | std::ios::trunc);
    if (!file) {
        return "{\"error\":\"" + jsonEscape(errnoMessage("open", path)) + "\"}";
    }

    file.write(content.data(), static_cast<std::streamsize>(content.size()));
    if (!file) {
        return "{\"error\":\"" + jsonEscape(errnoMessage("write", path)) + "\"}";
    }

    return "{\"written\":" + std::to_string(content.size()) + "}";
}

// file_exists
std::string doFileExists(const std::string& path) {
    if (fileExists(path)) {
        return "{\"exists\":true}";
    }
    return "{\"exists\":false}";
}

// file_edit: 基于行号的精确编辑
// startLine / endLine 均为 1-based，inclusive
std::string doFileEdit(const std::string& path,
                       int startLine, int endLine,
                       const std::string& newContent) {
    // 读取原文件
    auto [raw, error] = readFileRaw(path);
    if (!error.empty()) {
        return "{\"error\":\"" + jsonEscape(error) + "\"}";
    }

    auto lines = splitLines(raw);
    int totalLines = static_cast<int>(lines.size());

    // 参数校验
    if (startLine < 1) {
        return "{\"error\":\"start_line must be >= 1\"}";
    }
    if (endLine < startLine) {
        return "{\"error\":\"end_line must be >= start_line\"}";
    }

    int startIdx = startLine - 1; // 0-based
    int endIdx   = endLine - 1;

    // 构建新行数组
    std::vector<std::string> newLines;

    // (1) 保留 [0, startIdx) 的行
    int copyEnd = std::min(startIdx, totalLines);
    for (int i = 0; i < copyEnd; ++i) {
        newLines.push_back(lines[i]);
    }

    // (2) 如果 startIdx > totalLines，填充空行
    while (static_cast<int>(newLines.size()) < startIdx) {
        newLines.push_back("\n");
    }

    // (3) 插入新内容（按行拆分）
    auto insertLines = splitLines(newContent);
    for (auto& l : insertLines) {
        newLines.push_back(l);
    }

    // (4) 跳过原 [startIdx, endIdx] 的行，保留后续
    int afterEnd = endIdx + 1;
    if (afterEnd < totalLines) {
        for (int i = afterEnd; i < totalLines; ++i) {
            newLines.push_back(lines[i]);
        }
    }

    // 写入
    std::string writeErr = writeLines(path, newLines);
    if (!writeErr.empty()) {
        return "{\"error\":\"" + jsonEscape(writeErr) + "\"}";
    }

    int newTotal = static_cast<int>(newLines.size());
    return "{\"replaced_lines\":" + std::to_string(endLine - startLine + 1) + "," +
           "\"inserted_lines\":" + std::to_string(static_cast<int>(insertLines.size())) + "," +
           "\"total_lines\":" + std::to_string(newTotal) + "}";
}

// file_append: 追加写入
std::string doFileAppend(const std::string& path, const std::string& content) {
    std::ofstream file(path, std::ios::binary | std::ios::app);
    if (!file) {
        return "{\"error\":\"" + jsonEscape(errnoMessage("open", path)) + "\"}";
    }

    file.write(content.data(), static_cast<std::streamsize>(content.size()));
    if (!file) {
        return "{\"error\":\"" + jsonEscape(errnoMessage("write", path)) + "\"}";
    }

    return "{\"appended\":" + std::to_string(content.size()) + "}";
}

} // namespace

// ---- FilePlugin -------------------------------------------------------------

class FilePlugin final : public AFS::Plugin {
  public:
    // ---- lifecycle ----------------------------------------------------------
    const char* name() const override { return "file"; }
    AFS::PluginType type() const override { return AFS::PluginType::Tool; }
    void start() override {}
    void stop() override {}

    // ---- capabilities -------------------------------------------------------
    std::vector<ToolCap> toolCapabilities() const override {
        return {
            // ---- file_read (hashline 增强) ----------------------------------
            {"file_read",
             "Read a file from disk and return its content with hashline "
             "(line-number prefix) format (max 1 MiB). "
             "Input: {\"path\":\"/absolute/path\",\"offset\":1,\"limit\":200} "
             "— offset (1-based) and limit (max lines) are optional.",
             "{\"type\":\"object\","
             "\"properties\":{"
             "\"path\":{\"type\":\"string\","
             "\"description\":\"Absolute file path to read\"},"
             "\"offset\":{\"type\":\"integer\","
             "\"description\":\"1-based start line number (default 1)\"},"
             "\"limit\":{\"type\":\"integer\","
             "\"description\":\"Max lines to return (default 2000)\"}},"
             "\"required\":[\"path\"]}",
             [](const std::string& input) -> std::string {
                 std::string path;
                 if (!extractString(input, "path", path)) {
                     return "{\"error\":\"path is required and must be a string\"}";
                 }
                 if (path.empty()) return "{\"error\":\"path is empty\"}";

                 int offset = extractInt(input, "offset", 1);
                 int limit  = extractInt(input, "limit", DefaultLimit);
                 return doFileRead(path, offset, limit);
             }},

            // ---- file_write (全量覆盖) --------------------------------------
            {"file_write",
             "Write content to a file (truncates existing). "
             "Input: {\"path\":\"/absolute/path\",\"content\":\"...\"}.",
             "{\"type\":\"object\","
             "\"properties\":{"
             "\"path\":{\"type\":\"string\","
             "\"description\":\"Absolute file path to write\"},"
             "\"content\":{\"type\":\"string\","
             "\"description\":\"Content to write\"}},"
             "\"required\":[\"path\",\"content\"]}",
             [](const std::string& input) -> std::string {
                 std::string path;
                 if (!extractString(input, "path", path)) {
                     return "{\"error\":\"path is required and must be a string\"}";
                 }
                 if (path.empty()) return "{\"error\":\"path is empty\"}";

                 std::string content;
                 if (!extractString(input, "content", content)) {
                     return "{\"error\":\"content is required and must be a string\"}";
                 }
                 return doFileWrite(path, content);
             }},

            // ---- file_exists ------------------------------------------------
            {"file_exists",
             "Check whether a file exists. "
             "Input: {\"path\":\"/absolute/path\"}.",
             "{\"type\":\"object\","
             "\"properties\":{"
             "\"path\":{\"type\":\"string\","
             "\"description\":\"Absolute file path to check\"}},"
             "\"required\":[\"path\"]}",
             [](const std::string& input) -> std::string {
                 std::string path;
                 if (!extractString(input, "path", path)) {
                     return "{\"error\":\"path is required and must be a string\"}";
                 }
                 if (path.empty()) return "{\"error\":\"path is empty\"}";
                 return doFileExists(path);
             }},

            // ---- file_edit (基于 hash 行号的精确编辑) -----------------------
            {"file_edit",
             "Replace a line range in a file with new content. "
             "Lines are numbered 1-based (matching the hashline prefix from "
             "file_read). Input: {\"path\":\"/absolute/path\","
             "\"start_line\":10,\"end_line\":15,\"content\":\"new lines...\"}. "
             "start_line and end_line are inclusive.",
             "{\"type\":\"object\","
             "\"properties\":{"
             "\"path\":{\"type\":\"string\","
             "\"description\":\"Absolute file path to edit\"},"
             "\"start_line\":{\"type\":\"integer\","
             "\"description\":\"First line to replace (1-based, inclusive)\"},"
             "\"end_line\":{\"type\":\"integer\","
             "\"description\":\"Last line to replace (1-based, inclusive)\"},"
             "\"content\":{\"type\":\"string\","
             "\"description\":\"New content to insert in place of the range\"}},"
             "\"required\":[\"path\",\"start_line\",\"end_line\",\"content\"]}",
             [](const std::string& input) -> std::string {
                 std::string path;
                 if (!extractString(input, "path", path)) {
                     return "{\"error\":\"path is required and must be a string\"}";
                 }
                 if (path.empty()) return "{\"error\":\"path is empty\"}";

                 int startLine = extractInt(input, "start_line", -1);
                 int endLine   = extractInt(input, "end_line", -1);
                 if (startLine < 1) {
                     return "{\"error\":\"start_line is required and must be >= 1\"}";
                 }
                 if (endLine < 1) {
                     return "{\"error\":\"end_line is required and must be >= 1\"}";
                 }

                 std::string content;
                 if (!extractString(input, "content", content)) {
                     return "{\"error\":\"content is required and must be a string\"}";
                 }
                 return doFileEdit(path, startLine, endLine, content);
             }},

            // ---- file_append (追加写入) -------------------------------------
            {"file_append",
             "Append content to the end of a file (creates the file if it "
             "does not exist). Input: {\"path\":\"/absolute/path\","
             "\"content\":\"...\"}.",
             "{\"type\":\"object\","
             "\"properties\":{"
             "\"path\":{\"type\":\"string\","
             "\"description\":\"Absolute file path to append to\"},"
             "\"content\":{\"type\":\"string\","
             "\"description\":\"Content to append\"}},"
             "\"required\":[\"path\",\"content\"]}",
             [](const std::string& input) -> std::string {
                 std::string path;
                 if (!extractString(input, "path", path)) {
                     return "{\"error\":\"path is required and must be a string\"}";
                 }
                 if (path.empty()) return "{\"error\":\"path is empty\"}";

                 std::string content;
                 if (!extractString(input, "content", content)) {
                     return "{\"error\":\"content is required and must be a string\"}";
                 }
                 return doFileAppend(path, content);
             }},
        };
    }
};

// ---- C ABI exports ----------------------------------------------------------

AFS_PLUGIN_EXPORT std::uint32_t pluginAbiVersion() {
    return AFS::PluginAbiVersion;
}
AFS_PLUGIN_EXPORT AFS::Plugin* createPlugin() {
    return new FilePlugin();
}
AFS_PLUGIN_EXPORT void destroyPlugin(AFS::Plugin* p) {
    delete p;
}
