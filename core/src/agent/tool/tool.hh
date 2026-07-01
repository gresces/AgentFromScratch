#pragma once

#include <afs/tool.hh>

#include <functional>
#include <string>
#include <unordered_map>
#include <vector>

using AFS_ToolCall = AFS::ToolCall;
using AFS_ToolResult = AFS::ToolResult;
using AFS_ToolFunc = std::function<AFS_ToolResult(const AFS_ToolCall&)>;

// ---- AFS_ToolRegistry -------------------------------------------------------
// 工具注册与执行器。负责加载工具插件并执行工具调用。
class AFS_ToolRegistry final : public AFS::ToolExecutor {
  public:
    void registerTool(AFS::ToolSpec spec, AFS_ToolFunc func);
    AFS_ToolResult execute(const AFS_ToolCall& call) const override;
    bool hasTool(const std::string& name) const;
    std::vector<AFS::ToolSpec> listSpecs() const override;

  private:
    struct Entry {
        AFS::ToolSpec spec;
        AFS_ToolFunc func;
    };
    std::unordered_map<std::string, Entry> tools_;
};
