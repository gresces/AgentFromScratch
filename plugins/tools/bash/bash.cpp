#include <afs.hh>

#include <array>
#include <cerrno>
#include <cstdio>
#include <cstring>
#include <string>
#include <sys/wait.h>
#include <unistd.h>
#include <utility>

namespace {

// ---- constants --------------------------------------------------------------

constexpr size_t BufferSize = 4096;

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

// ---- process helpers --------------------------------------------------------

struct ExecResult {
    std::string output;
    std::string error;
    int exit_code = -1;
};

std::string errnoMessage(const char* operation) {
    return std::string(operation) + " failed in bash tool: " + std::strerror(errno);
}

void closeIfOpen(int fd) {
    if (fd >= 0) close(fd);
}

int waitForChild(pid_t pid) {
    int status = 0;
    while (waitpid(pid, &status, 0) == -1) {
        if (errno == EINTR) continue;
        return -1;
    }

    if (WIFEXITED(status)) return WEXITSTATUS(status);
    if (WIFSIGNALED(status)) return 128 + WTERMSIG(status);
    return status;
}

ExecResult execCommand(const std::string& command) {
    int pipe_fds[2] = {-1, -1};
    if (pipe(pipe_fds) == -1) {
        return ExecResult{.output = "", .error = errnoMessage("pipe"), .exit_code = -1};
    }

    pid_t pid = fork();
    if (pid == -1) {
        std::string error = errnoMessage("fork");
        closeIfOpen(pipe_fds[0]);
        closeIfOpen(pipe_fds[1]);
        return ExecResult{.output = "", .error = std::move(error), .exit_code = -1};
    }

    if (pid == 0) {
        closeIfOpen(pipe_fds[0]);
        if (dup2(pipe_fds[1], STDOUT_FILENO) == -1) _exit(127);
        if (dup2(pipe_fds[1], STDERR_FILENO) == -1) _exit(127);
        closeIfOpen(pipe_fds[1]);
        execl("/bin/bash", "bash", "-lc", command.c_str(), static_cast<char*>(nullptr));
        _exit(127);
    }

    closeIfOpen(pipe_fds[1]);

    ExecResult result;
    std::array<char, BufferSize> buffer = {};
    while (true) {
        ssize_t bytes_read = read(pipe_fds[0], buffer.data(), buffer.size());
        if (bytes_read > 0) {
            result.output.append(buffer.data(), static_cast<size_t>(bytes_read));
            continue;
        }
        if (bytes_read == 0) break;
        if (errno == EINTR) continue;

        result.error = errnoMessage("read");
        break;
    }

    closeIfOpen(pipe_fds[0]);
    result.exit_code = waitForChild(pid);
    if (result.exit_code == -1 && result.error.empty()) {
        result.error = errnoMessage("waitpid");
    }
    return result;
}

// ---- response helpers -------------------------------------------------------

std::string resultToJson(const ExecResult& result) {
    std::string response = "{\"output\":\"" + jsonEscape(result.output) + "\",";
    response += "\"exit_code\":" + std::to_string(result.exit_code);
    if (!result.error.empty()) {
        response += ",\"error\":\"" + jsonEscape(result.error) + "\"";
    }
    response += "}";
    return response;
}

} // namespace

// ---- BashPlugin -------------------------------------------------------------

class BashPlugin final : public AFS::Plugin {
  public:
    // ---- lifecycle ----------------------------------------------------------
    const char* name() const override { return "bash"; }
    AFS::PluginType type() const override { return AFS::PluginType::Tool; }
    void start() override {}
    void stop() override {}

    // ---- capabilities -------------------------------------------------------
    std::vector<ToolCap> toolCapabilities() const override {
        return {
            {"bash",
             "Execute a shell command via /bin/bash -lc and return combined "
             "stdout+stderr plus the shell-compatible exit code.",
             R"({"type":"object","properties":{"command":{"type":"string","description":"Shell command to execute with /bin/bash -lc"}},"required":["command"]})",
             [](const std::string& input) -> std::string {
                 std::string command;
                 if (!extractString(input, "command", command)) {
                     return R"({"error":"command is required and must be a string"})";
                 }
                 if (command.empty()) return R"({"error":"command is empty"})";

                 return resultToJson(execCommand(command));
             }},
        };
    }
};

// ---- C ABI exports ----------------------------------------------------------

AFS_PLUGIN_EXPORT std::uint32_t pluginAbiVersion() {
    return AFS::PluginAbiVersion;
}
AFS_PLUGIN_EXPORT AFS::Plugin* createPlugin() {
    return new BashPlugin();
}
AFS_PLUGIN_EXPORT void destroyPlugin(AFS::Plugin* p) {
    delete p;
}
