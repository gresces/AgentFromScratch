#pragma once

#include "agent/loop/loop.hh"
#include "agent/tool/tool.hh"
#include "agent/context/context.hh"
#include "basic/models/model.hh"
#include "afs/plugin.hh"

#include <memory>
#include <string>
#include <vector>

// ---- AFS_Agent ---------------------------------------------------------------
// Agent 核心节点，程序中的 Agent 之间为树状结构。
//
// 规则：
//   1. 一个程序有且只有一个主 Agent（level == 0）。
//   2. 父节点通过 unique_ptr 独占子节点所有权。
//   3. genSubNode() 返回引用（非拥有），只有父节点有权操作子节点。
//   4. 删除一个节点时，其所有后代节点递归销毁（unique_ptr 析构保证）。
//   5. 子 Agent 继承父 Agent 的已注册工具。
//   6. 所有 Agent 构造时自动初始化默认上下文（系统提示词），仅由 Agent 自身管理。
//   7. 主 Agent（level==0）构造时自动调用 registerTools()。
//
// Agent 拥有三个核心组件：
//   - AFS_Loop：   驱动 LLM 交互和工具调用的状态机。
//   - AFS_Context：管理对话消息历史。
//   - AFS_Model：  执行实际的 LLM API 调用。
class AFS_Agent {
  public:
    // ---- lifecycle ----------------------------------------------------------
    static std::unique_ptr<AFS_Agent> createMain();

    AFS_Agent(const AFS_Agent&) = delete;
    AFS_Agent& operator=(const AFS_Agent&) = delete;

    ~AFS_Agent();

    // ---- execution ----------------------------------------------------------
    // 设置此 Agent 使用的模型（转移所有权）。
    void setModel(std::unique_ptr<AFS_Model> model);

    // 运行 Agent 循环：使用当前上下文和模型与 LLM 交互直到获得最终回复。
    std::string run();

    // ---- tree mutation ------------------------------------------------------
    AFS_Agent& genSubNode();

    void killSubNode(AFS_Agent& node);

    static std::unique_ptr<AFS_Agent> swapWithChild(std::unique_ptr<AFS_Agent> self,
                                                    AFS_Agent& child);

    // ---- tool management ----------------------------------------------------
    // 从插件管理器注册所有已加载工具插件（主 Agent 启动时调用）。
    void registerTools();

    // 加载指定插件并注册其工具（子 Agent 扩展能力）。
    void loadExtraTool(AFS::PluginType type, const std::string& plugin_name);

    // 移除已注册的工具（同时通知插件管理器释放引用）。
    void removeTool(const std::string& tool_name);

    // ---- accessors ----------------------------------------------------------
    unsigned level() const { return level_; }
    bool isMain() const { return level_ == 0; }
    size_t subCount() const { return sub_agent_nodes_.size(); }
    const std::string& uuid() const { return uuid_; }
    const AFS_ToolRegistry& toolRegistry() const { return tool_registry_; }
    AFS_ToolRegistry& toolRegistry() { return tool_registry_; }
    AFS_Context& context() { return context_; }
    const AFS_Context& context() const { return context_; }
    const AFS_Model* model() const { return model_.get(); }

    // ---- logging ------------------------------------------------------------

    // ---- diagnostics --------------------------------------------------------
    static std::string printMain(const AFS_Agent& root);

  private:
    explicit AFS_Agent(unsigned level);
    // 子 Agent 构造：继承父级的工具注册表，上下文由父级后续显式设置
    AFS_Agent(unsigned level, const AFS_ToolRegistry& parent_tools);

    static void printNode(std::string& out, const AFS_Agent& node, const std::string& prefix,
                          bool is_last);
    static void fixSubtreeLevels(AFS_Agent& node);

    // 初始化默认上下文（系统提示词等）。
    void initDefaultContext();

    unsigned level_ = 0;
    std::string uuid_;
    std::vector<std::unique_ptr<AFS_Agent>> sub_agent_nodes_;
    AFS_ToolRegistry tool_registry_;
    AFS_Context context_;
    std::unique_ptr<AFS_Model> model_;
    AFS_Loop loop_;
    // 本 Agent 加载的插件列表（用于析构时释放引用计数）
    std::vector<std::pair<AFS::PluginType, std::string>> loaded_plugins_;
};
