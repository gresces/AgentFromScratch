#pragma once

#include "context.hh"
#include "model.hh"
#include "tool.hh"

#include <string>

struct AFS_ConfigSchema;

namespace AFS {

// ---- LoopConfig --------------------------------------------------------------
struct LoopConfig {
    unsigned max_iterations = 50;

    static AFS_ConfigSchema configSchema();
};

// ---- LoopEvents --------------------------------------------------------------
class LoopEvents {
  public:
    virtual ~LoopEvents() = default;

    virtual void publishStart() = 0;
    virtual void publishAssistantMessage(const std::string& message_print) = 0;
    virtual void publishAssistantDelta(const std::string& delta) = 0;
    virtual void publishReasoningMessage(const std::string& reasoning) = 0;
    virtual void publishReasoningDelta(const std::string& delta) = 0;
    virtual void publishToolResult(const std::string& message_print) = 0;
    virtual void publishError(const std::string& agent_uuid, const std::string& error) = 0;
    virtual void publishComplete(const std::string& reply) = 0;
};

// ---- Loop --------------------------------------------------------------------
// Runtime loop interface implemented by a loop-type plugin.
// The loop mutates Context by appending assistant/tool messages while it runs.
class Loop {
  public:
    virtual ~Loop() = default;

    // ---- configuration ------------------------------------------------------
    virtual void setConfig(LoopConfig config) = 0;
    virtual const LoopConfig& config() const = 0;

    // ---- execution ----------------------------------------------------------
    virtual std::string run(Context& context, ToolExecutor& tools, const Model& model,
                            LoopEvents& events, const std::string& agent_uuid) = 0;
};

} // namespace AFS
