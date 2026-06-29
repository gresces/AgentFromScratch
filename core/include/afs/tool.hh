#pragma once

#include "afs/metadata.hh"

#include <string>
#include <unordered_map>

namespace AFS {

// ---- ToolSpec ---------------------------------------------------------------
// 插件开发者需要了解的工具描述结构。
// 其它运行时类型（ToolCall、ToolResult、ToolRegistry）属于 Agent 内部实现，
// 定义在 core/src/agent/tool/ 中。
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

} // namespace AFS
