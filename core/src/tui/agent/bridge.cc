#include "tui/agent/bridge.hh"

#include "basic/config/config.hh"
#include "basic/config/paths.hh"
#include "basic/log/logger.hh"
#include "basic/models/model.hh"
#include "plugins/plugin_manager.hh"

#include <filesystem>
#include <string_view>
#include <thread>

namespace {

std::string shortWorkingDirectory() {
    auto cwd = std::filesystem::current_path();
    std::string short_dir;
    auto it = cwd.end();
    if (it != cwd.begin()) {
        --it;
        short_dir = it->string();
        if (it != cwd.begin()) {
            --it;
            short_dir = it->string() + "/" + short_dir;
        }
    }
    return short_dir;
}

std::string stripMessagePrefix(const std::string& text, const char* prefix) {
    if (text.starts_with(prefix)) return text.substr(std::char_traits<char>::length(prefix));
    return text;
}

TuiMessage parseToolMessage(const std::string& text) {
    constexpr std::string_view prefix = "[tool ";
    if (!text.starts_with(prefix)) return {TuiMessage::Tool, text, ""};

    auto end = text.find("] ", prefix.size());
    if (end == std::string::npos) return {TuiMessage::Tool, text, ""};

    return {
        TuiMessage::Tool,
        text.substr(end + 2),
        text.substr(prefix.size(), end - prefix.size()),
    };
}

} // namespace

std::unique_ptr<AFS_TuiAgentBridge> AFS_TuiAgentBridge::create(const std::string& config_path) {
    AFS_Logger::init();

    auto config = AFS_Config::loadFromFile(config_path);
    if (!config || config->models().llms.empty()) {
        AFS_LOG_ERROR(kRoleMain, "", "failed to load config or no LLM model is configured");
        return nullptr;
    }

    AFS_PluginManager::instance()->loadFromDirectory(AFS_DefaultPluginDirectory());

    auto agent = AFS_Agent::createMain();
    if (!agent) {
        AFS_LOG_ERROR(kRoleMain, "", "failed to create Agent");
        return nullptr;
    }

    agent->setModel(createModel(config->models().llms[0]));

    auto bridge = std::unique_ptr<AFS_TuiAgentBridge>(new AFS_TuiAgentBridge());
    bridge->agent_ = std::move(agent);
    bridge->model_name_ = config->models().llms[0].model;
    bridge->work_dir_ = shortWorkingDirectory();
    return bridge;
}

bool AFS_TuiAgentBridge::submitUserMessage(const std::string& content) {
    if (content.empty() || running_.load()) return false;

    agent_->context().addMessage(AFS::UserMessage(content));

    running_.store(true);
    std::thread([this]() {
        agent_->run();
        running_.store(false);
    }).detach();
    return true;
}

std::vector<TuiMessage> AFS_TuiAgentBridge::pollMessages() {
    auto events = AFS_Logger::instance().poll();
    std::vector<TuiMessage> messages;
    messages.reserve(events.size());

    for (const auto& event : events) {
        switch (event.type) {
        case AgentEvent::Start:
            break;
        case AgentEvent::AssistantMessage:
            if (event.message_print)
                messages.push_back({TuiMessage::Assistant,
                                    stripMessagePrefix(*event.message_print, "[assistant] "), ""});
            break;
        case AgentEvent::ToolResult:
            if (event.message_print) messages.push_back(parseToolMessage(*event.message_print));
            break;
        case AgentEvent::Error:
            messages.push_back({TuiMessage::Assistant, "[error] " + event.text, ""});
            break;
        case AgentEvent::Complete:
            if (!event.text.empty()) messages.push_back({TuiMessage::Assistant, event.text, ""});
            break;
        }
    }

    return messages;
}
