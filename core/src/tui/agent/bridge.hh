#pragma once

#include "agent/agent.hh"
#include "tui/message/message.hh"

#include <atomic>
#include <memory>
#include <string>
#include <vector>

class AFS_TuiAgentBridge {
  public:
    static std::unique_ptr<AFS_TuiAgentBridge> create(const std::string& config_path);

    const std::string& modelName() const { return model_name_; }
    const std::string& workDir() const { return work_dir_; }
    bool running() const { return running_.load(); }
    std::size_t messageCount() const { return agent_->context().messageCount(); }

    bool submitUserMessage(const std::string& content);
    std::vector<TuiMessage> pollMessages();

  private:
    AFS_TuiAgentBridge() = default;

    std::unique_ptr<AFS_Agent> agent_;
    std::string model_name_;
    std::string work_dir_;
    std::atomic<bool> running_{false};
};
