#pragma once

#include <optional>
#include <nlohmann/json.hpp>
#include <string>
#include <unordered_map>

#include "metadata.hh"

namespace AFS {

// ---- Role -------------------------------------------------------------------
enum class Role {
    System,
    Developer,
    User,
    Assistant,
    Tool,
};

inline const char* roleName(Role role) {
    switch (role) {
    case Role::System:
        return "system";
    case Role::Developer:
        return "developer";
    case Role::User:
        return "user";
    case Role::Assistant:
        return "assistant";
    case Role::Tool:
        return "tool";
    }
    return "unknown";
}

// ---- Message ----------------------------------------------------------------
struct Message {
    Role role;
    std::string content;
    std::optional<std::string> name;
    std::optional<std::string> tool_call_id;
    std::optional<nlohmann::json> tool_calls; // Assistant 消息中的 tool_calls
    std::string reasoning_content;            // 模型的思考过程（DeepSeek R1 等）
    std::unordered_map<std::string, std::string> metadata;

    std::string print() const {
        std::string out;
        out += "[";
        out += roleName(role);

        if (name.has_value()) out += " name=" + *name;
        if (tool_call_id.has_value()) out += " call_id=" + *tool_call_id;

        out += "] ";
        out += content;

        // 输出思考过程
        if (!reasoning_content.empty()) {
            out += "\n  ";
            out += reasoning_content;
        }

        // 输出工具调用
        if (tool_calls.has_value()) {
            for (const auto& tc : tool_calls.value()) {
                out += "\n  → " + tc["function"]["name"].get<std::string>() + "(" +
                       tc["function"]["arguments"].get<std::string>() + ")";
            }
        }

        appendMeta(out, metadata);
        return out;
    }
};

// ---- 便捷类型 ---------------------------------------------------------------
struct SystemMessage : Message {
    SystemMessage() { role = Role::System; }
    explicit SystemMessage(std::string c) : Message{Role::System, std::move(c)} {}
};

struct DeveloperMessage : Message {
    DeveloperMessage() { role = Role::Developer; }
    explicit DeveloperMessage(std::string c) : Message{Role::Developer, std::move(c)} {}
};

struct UserMessage : Message {
    UserMessage() { role = Role::User; }
    explicit UserMessage(std::string c) : Message{Role::User, std::move(c)} {}
};

struct AssistantMessage : Message {
    AssistantMessage() { role = Role::Assistant; }
    explicit AssistantMessage(std::string c) : Message{Role::Assistant, std::move(c)} {}
};

struct ToolMessage : Message {
    ToolMessage() { role = Role::Tool; }
    explicit ToolMessage(std::string c) : Message{Role::Tool, std::move(c)} {}
};

} // namespace AFS
