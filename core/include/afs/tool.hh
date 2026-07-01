#pragma once

#include "afs/common.hh"
#include "afs/metadata.hh"

#include <cstddef>
#include <string>
#include <vector>
#include <unordered_map>

namespace AFS {

// ---- ToolSpec ---------------------------------------------------------------
// 插件开发者需要了解的工具描述结构。Loop 插件还会通过 ToolExecutor 查询与执行工具。
struct ToolSpec {
    std::string name;         // 工具名称
    std::string description;  // 功能描述
    std::string input_schema; // JSON Schema 输入参数定义
    std::unordered_map<std::string, std::string> metadata;

    std::string print() const {
        std::string out;
        out += "[ToolSpec] " + name + ": " + description;
        if (!input_schema.empty()) {
            out += " schema=" + input_schema;
        }
        appendMeta(out, metadata);
        return out;
    }
};

// ---- ToolCall ---------------------------------------------------------------
struct ToolCall {
    std::string uuid;
    std::string name;
    std::string arguments;
    std::vector<std::string> environment;
    std::unordered_map<std::string, std::string> metadata;

    ToolCall() : uuid(uuid16()) {}

    std::string print() const {
        std::string out;
        out += "[ToolCall " + uuid + "] " + name + "(" + arguments + ")";
        if (!environment.empty()) {
            out += " env=[";
            for (size_t i = 0; i < environment.size(); ++i) {
                if (i > 0) out += ", ";
                out += environment[i];
            }
            out += "]";
        }
        appendMeta(out, metadata);
        return out;
    }
};

// ---- ToolResult -------------------------------------------------------------
struct ToolResult {
    std::string call_uuid;
    std::string tool_name;
    bool success = false;
    std::string output;
    std::string error;
    std::unordered_map<std::string, std::string> metadata;

    std::string print() const {
        std::string out;
        out += "[ToolResult " + call_uuid + "] " + tool_name + " ";
        out += success ? "OK: " + output : "FAIL: " + error;
        appendMeta(out, metadata);
        return out;
    }
};

// ---- ToolExecutor -----------------------------------------------------------
class ToolExecutor {
  public:
    virtual ~ToolExecutor() = default;

    virtual ToolResult execute(const ToolCall& call) const = 0;
    virtual std::vector<ToolSpec> listSpecs() const = 0;
};

} // namespace AFS
