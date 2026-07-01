#pragma once

#include "agent/context/context.hh"
#include "agent/tool/tool.hh"
#include "basic/config/config.hh"
#include "basic/models/model.hh"

#include <optional>
#include <string>
#include <vector>

#include <nlohmann/json_fwd.hpp>

// ---- AFS_AgentLoopConfig ------------------------------------------------------
struct AFS_AgentLoopConfig {
    unsigned max_iterations = 50;

    static AFS_ConfigSchema configSchema();
};

void from_json(const nlohmann::json& j, AFS_AgentLoopConfig& config);
void AFS_RegisterAgentConfigSchemas();
std::optional<AFS_AgentLoopConfig> AFS_LoadAgentLoopConfig(const AFS_Config& config);
std::optional<AFS_AgentLoopConfig> AFS_LoadAgentLoopConfig(const AFS_ConfigManager& manager);
std::optional<AFS_AgentLoopConfig> AFS_LoadAgentLoopConfig();

// ---- AFS_Loop ----------------------------------------------------------------
// Agent 核心运行循环，基于 boost::sml 状态机，作为 AFS_Agent 的私有成员。
// 仅依赖运行所需的四个资源，不依赖整个 Agent。
class AFS_Loop {
  public:
    void setConfig(AFS_AgentLoopConfig config) { config_ = config; }
    const AFS_AgentLoopConfig& config() const { return config_; }

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

  private:
    AFS_AgentLoopConfig config_;
};
