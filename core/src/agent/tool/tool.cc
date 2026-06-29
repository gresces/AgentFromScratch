#include "agent/tool/tool.hh"

// ---- AFS_ToolCall -----------------------------------------------------------

AFS_ToolCall::AFS_ToolCall() : uuid(AFS::uuid16()) {
}

std::string AFS_ToolCall::print() const {
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

// ---- AFS_ToolResult ---------------------------------------------------------

std::string AFS_ToolResult::print() const {
    std::string out;
    out += "[ToolResult " + call_uuid + "] " + tool_name + " ";
    out += success ? "OK: " + output : "FAIL: " + error;
    appendMeta(out, metadata);
    return out;
}

// ---- AFS_ToolRegistry -------------------------------------------------------

void AFS_ToolRegistry::registerTool(AFS::ToolSpec spec, AFS_ToolFunc func) {
    std::string name = spec.name;
    tools_.emplace(std::move(name), Entry{std::move(spec), std::move(func)});
}

AFS_ToolResult AFS_ToolRegistry::execute(const AFS_ToolCall& call) const {
    auto it = tools_.find(call.name);
    if (it == tools_.end()) {
        AFS_ToolResult result;
        result.call_uuid = call.uuid;
        result.tool_name = call.name;
        result.success = false;
        result.error = "tool not registered: " + call.name;
        return result;
    }

    AFS_ToolResult result = it->second.func(call);
    result.call_uuid = call.uuid;
    result.tool_name = call.name;
    return result;
}

bool AFS_ToolRegistry::hasTool(const std::string& name) const {
    return tools_.find(name) != tools_.end();
}

std::vector<AFS::ToolSpec> AFS_ToolRegistry::listSpecs() const {
    std::vector<AFS::ToolSpec> specs;
    specs.reserve(tools_.size());
    for (const auto& [name, entry] : tools_) {
        specs.push_back(entry.spec);
    }
    return specs;
}
