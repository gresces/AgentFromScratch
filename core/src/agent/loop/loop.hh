#pragma once

#include "agent/agent.hh"
#include "basic/models/model.hh"

#include <string>

// ---- AFS_Loop ----------------------------------------------------------------
// Agent 核心运行循环，基于 boost::sml 状态机。
// 负责将上下文发送到 LLM，解析响应，执行工具调用，循环直至完成。
class AFS_Loop {
  public:
    // 最大迭代次数（防止无限循环）
    static constexpr unsigned kMaxIterations = 10;

    // 运行完整循环：从 agent 的上下文出发，与 LLM 交互直到获得最终回复。
    // 返回最终回复内容，失败时返回空字符串。
    std::string run(AFS_Agent& agent, AFS_Model& model);
    // 解析 LLM 响应，提取文本内容和工具调用
    struct ParsedResponse {
        std::string content;                    // 文本回复
        std::string reasoning;                  // 思考过程
        std::vector<nlohmann::json> tool_calls; // 工具调用列表
        bool hasToolCalls() const { return !tool_calls.empty(); }
    };
};
