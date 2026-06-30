#pragma once

#include "agent/context/context.hh"
#include "agent/tool/tool.hh"
#include "basic/models/model.hh"

#include <string>
#include <vector>

#include <nlohmann/json_fwd.hpp>

// ---- AFS_Loop ----------------------------------------------------------------
// Agent 核心运行循环，基于 boost::sml 状态机，作为 AFS_Agent 的私有成员。
// 仅依赖运行所需的四个资源，不依赖整个 Agent。
class AFS_Loop {
  public:
    // 最大迭代次数（防止无限循环）
    static constexpr unsigned kMaxIterations = 10;

    // 运行完整循环：与 LLM 交互直到获得最终回复。
    //   context — 对话历史，循环中会追加 Assistant/Tool 消息
    //   tools   — 工具注册表，用于执行 LLM 请求的工具调用
    //   model   — LLM 模型，用于 chatCompletion 调用
    //   agent_uuid — 用于日志标识（也用于 Logger 日志）
    // 返回最终回复内容，失败时返回空字符串。
    std::string run(AFS_Context& context, AFS_ToolRegistry& tools, const AFS_Model& model,
                    const std::string& agent_uuid);

    // 解析 LLM 响应，提取文本内容和工具调用
    struct ParsedResponse {
        std::string content;                    // 文本回复
        std::string reasoning;                  // 思考过程
        std::vector<nlohmann::json> tool_calls; // 工具调用列表
        bool hasToolCalls() const { return !tool_calls.empty(); }
    };
};
