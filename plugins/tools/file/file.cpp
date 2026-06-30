#include <afs.hh>

#include <cerrno>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <sstream>
#include <string>
#include <sys/stat.h>

namespace {

// ---- constants --------------------------------------------------------------

constexpr size_t MaxReadSize = 1024 * 1024; // 1 MiB

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
    case '"':
        value += '"';
        return true;
    case '\\':
        value += '\\';
        return true;
    case '/':
        value += '/';
        return true;
    case 'b':
        value += '\b';
        return true;
    case 'f':
        value += '\f';
        return true;
    case 'n':
        value += '\n';
        return true;
    case 'r':
        value += '\r';
        return true;
    case 't':
        value += '\t';
        return true;
    default:
        return false;
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
        case '"':
            output += "\\\"";
            break;
        case '\\':
            output += "\\\\";
            break;
        case '\b':
            output += "\\b";
            break;
        case '\f':
            output += "\\f";
            break;
        case '\n':
            output += "\\n";
            break;
        case '\r':
            output += "\\r";
            break;
        case '\t':
            output += "\\t";
            break;
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

std::string readFile(const std::string& path) {
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file) {
        return "{\"error\":\"" + jsonEscape(errnoMessage("open", path)) + "\"}";
    }

    std::streamsize size = file.tellg();
    if (size < 0) {
        return "{\"error\":\"failed to determine file size\"}";
    }
    if (static_cast<size_t>(size) > MaxReadSize) {
        return "{\"error\":\"file too large (max 1 MiB)\"}";
    }

    file.seekg(0, std::ios::beg);
    std::string content(static_cast<size_t>(size), '\0');
    if (!file.read(content.data(), size)) {
        return "{\"error\":\"" + jsonEscape(errnoMessage("read", path)) + "\"}";
    }

    return "{\"content\":\"" + jsonEscape(content) + "\",\"size\":" + std::to_string(size) + "}";
}

std::string writeFile(const std::string& path, const std::string& content) {
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

std::string checkExists(const std::string& path) {
    if (fileExists(path)) {
        return "{\"exists\":true}";
    }
    return "{\"exists\":false}";
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
            {"file_read",
             "Read a file from disk and return its content (max 1 MiB). "
             "Input: {\"path\":\"/absolute/path\"}.",
             "{\"type\":\"object\","
             "\"properties\":{"
             "\"path\":{\"type\":\"string\","
             "\"description\":\"Absolute file path to read\"}},"
             "\"required\":[\"path\"]}",
             [](const std::string& input) -> std::string {
                 std::string path;
                 if (!extractString(input, "path", path)) {
                     return "{\"error\":\"path is required\"}";
                 }
                 if (path.empty()) return "{\"error\":\"path is empty\"}";
                 return readFile(path);
             }},

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
                     return "{\"error\":\"path is required\"}";
                 }
                 if (path.empty()) return "{\"error\":\"path is empty\"}";

                 std::string content;
                 if (!extractString(input, "content", content)) {
                     return "{\"error\":\"content is required\"}";
                 }
                 return writeFile(path, content);
             }},

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
                     return "{\"error\":\"path is required\"}";
                 }
                 if (path.empty()) return "{\"error\":\"path is empty\"}";
                 return checkExists(path);
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
