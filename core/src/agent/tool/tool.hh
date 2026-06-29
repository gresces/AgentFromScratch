#pragma once

#include "afs/tool.hh"
#include "afs/common.hh"

#include <functional>
#include <string>
#include <unordered_map>
#include <vector>

// ---- AFS_ToolCall -----------------------------------------------------------
// Agent 发起的单次工具调用请求（内部类型，不导出给插件开发者）。
struct AFS_ToolCall {
    std::string uuid;
    std::string name;
    std::string arguments;
    std::vector<std::string> environment;
    std::unordered_map<std::string, std::string> metadata;

    AFS_ToolCall();

    std::string print() const;
};

// ---- AFS_ToolResult ---------------------------------------------------------
// 工具调用执行结果（内部类型）。
struct AFS_ToolResult {
    std::string call_uuid;
    std::string tool_name;
    bool success = false;
    std::string output;
    std::string error;
    std::unordered_map<std::string, std::string> metadata;

    std::string print() const;
};

// ---- AFS_ToolFunc -----------------------------------------------------------
using AFS_ToolFunc = std::function<AFS_ToolResult(const AFS_ToolCall&)>;

// ---- AFS_ToolRegistry -------------------------------------------------------
// 工具注册与执行器。负责加载工具插件并执行工具调用。
class AFS_ToolRegistry {
  public:
    void registerTool(AFS::ToolSpec spec, AFS_ToolFunc func);
    AFS_ToolResult execute(const AFS_ToolCall& call) const;
    bool hasTool(const std::string& name) const;
    std::vector<AFS::ToolSpec> listSpecs() const;

  private:
    struct Entry {
        AFS::ToolSpec spec;
        AFS_ToolFunc func;
    };
    std::unordered_map<std::string, Entry> tools_;
};
