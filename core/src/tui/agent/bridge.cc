#include "tui/agent/bridge.hh"

#include "basic/config/config.hh"
#include "basic/config/paths.hh"
#include "basic/log/logger.hh"
#include "basic/models/model.hh"
#include "plugins/plugin_manager.hh"

#include <nlohmann/json.hpp>

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

std::string formatJsonValueForDisplay(const nlohmann::json& value);

std::string formatJsonObjectForDisplay(const nlohmann::json& object) {
    std::string output;
    bool first = true;
    for (const auto& [key, value] : object.items()) {
        if (!first) output += "\n";
        output += key + ": ";
        std::string rendered = formatJsonValueForDisplay(value);
        if (value.is_string() && rendered.find('\n') != std::string::npos) {
            output += "\n";
        }
        output += rendered;
        first = false;
    }
    return output;
}

std::string formatJsonValueForDisplay(const nlohmann::json& value) {
    if (value.is_string()) return value.get<std::string>();
    if (value.is_object()) return formatJsonObjectForDisplay(value);
    if (value.is_array()) {
        std::string output;
        for (size_t i = 0; i < value.size(); ++i) {
            if (i > 0) output += "\n";
            output += "- " + formatJsonValueForDisplay(value[i]);
        }
        return output;
    }
    return value.dump();
}

std::string formatToolContentForDisplay(const std::string& content) {
    try {
        return formatJsonValueForDisplay(nlohmann::json::parse(content));
    } catch (const nlohmann::json::parse_error&) {
        return content;
    }
}

TuiMessage parseToolMessage(const std::string& text) {
    constexpr std::string_view prefix = "[tool ";
    if (!text.starts_with(prefix)) return {TuiMessage::Tool, formatToolContentForDisplay(text), ""};

    auto end = text.find("] ", prefix.size());
    if (end == std::string::npos) return {TuiMessage::Tool, formatToolContentForDisplay(text), ""};

    return {
        TuiMessage::Tool,
        formatToolContentForDisplay(text.substr(end + 2)),
        text.substr(prefix.size(), end - prefix.size()),
    };
}

} // namespace

std::unique_ptr<AFS_TuiAgentBridge> AFS_TuiAgentBridge::create(const std::string& config_path) {
    AFS_Logger::init();

    auto& config_manager = AFS_ConfigManager::instance();
    AFS_RegisterModelConfigSchemas();
    AFS_RegisterAgentConfigSchemas();
    AFS_RegisterPluginConfigSchemas();
    std::string config_error;
    if (!config_manager.loadFromFile(config_path, config_error)) {
        AFS_LOG_ERROR(kRoleMain, "", "failed to load config: " + config_error);
        return nullptr;
    }
    auto models = AFS_LoadModelsConfig(config_manager);
    if (!models || models->llms.empty()) {
        AFS_LOG_ERROR(kRoleMain, "", "failed to load config or no LLM model is configured");
        return nullptr;
    }
    auto plugin_config = AFS_LoadPluginRuntimeConfig(config_manager);
    AFS_PluginManager::instance()->loadFromDirectory(
        plugin_config ? std::filesystem::path(plugin_config->directory)
                      : AFS_DefaultPluginDirectory());

    auto agent = AFS_Agent::createMain();
    if (!agent) {
        AFS_LOG_ERROR(kRoleMain, "", "failed to create Agent");
        return nullptr;
    }

    agent->setModel(createModel(models->llms[0]));
    if (auto loop_config = AFS_LoadAgentLoopConfig(config_manager)) {
        agent->setLoopConfig(*loop_config);
    }

    auto bridge = std::unique_ptr<AFS_TuiAgentBridge>(new AFS_TuiAgentBridge());
    bridge->agent_ = std::move(agent);
    bridge->model_name_ = models->llms[0].model;
    bridge->work_dir_ = shortWorkingDirectory();
    bridge->context_limit_ = models->llms[0].context_limit;
    return bridge;
}
bool AFS_TuiAgentBridge::reloadConfig(const std::string& config_path) {
    if (running_.load()) return false;

    auto& config_manager = AFS_ConfigManager::instance();
    AFS_RegisterModelConfigSchemas();
    AFS_RegisterAgentConfigSchemas();
    AFS_RegisterPluginConfigSchemas();
    std::string config_error;
    if (!config_manager.loadFromFile(config_path, config_error)) {
        AFS_LOG_ERROR(kRoleMain, "", "failed to reload config: " + config_error);
        return false;
    }
    auto models = AFS_LoadModelsConfig(config_manager);
    if (!models || models->llms.empty()) {
        AFS_LOG_ERROR(kRoleMain, "", "failed to reload config or no LLM model is configured");
        return false;
    }

    agent_->setModel(createModel(models->llms[0]));
    if (auto loop_config = AFS_LoadAgentLoopConfig(config_manager)) {
        agent_->setLoopConfig(*loop_config);
    }
    model_name_ = models->llms[0].model;
    context_limit_ = models->llms[0].context_limit;
    return true;
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
        case AgentEvent::AssistantDelta:
            if (!event.text.empty())
                messages.push_back({TuiMessage::Assistant, event.text, "", true});
            break;
        case AgentEvent::ReasoningMessage:
            if (!event.text.empty()) messages.push_back({TuiMessage::Thinking, event.text, ""});
            break;
        case AgentEvent::ReasoningDelta:
            if (!event.text.empty())
                messages.push_back({TuiMessage::Thinking, event.text, "", true});
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
