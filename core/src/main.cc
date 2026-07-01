#include "basic/log/logger.hh"
#include "agent/agent.hh"
#include "basic/config/config.hh"
#include "basic/config/paths.hh"
#include "basic/models/model.hh"
#include "plugins/plugin_manager.hh"
#include "tui/app.hh"

#include <nlohmann/json.hpp>

#include <atomic>
#include <chrono>
#include <thread>
#include <iostream>

namespace {

std::string formatJsonValueForConsole(const nlohmann::json& value);

std::string formatJsonObjectForConsole(const nlohmann::json& object) {
    std::string output;
    bool first = true;
    for (const auto& [key, value] : object.items()) {
        if (!first) output += "\n";
        output += key + ": ";
        std::string rendered = formatJsonValueForConsole(value);
        if (value.is_string() && rendered.find('\n') != std::string::npos) output += "\n";
        output += rendered;
        first = false;
    }
    return output;
}

std::string formatJsonValueForConsole(const nlohmann::json& value) {
    if (value.is_string()) return value.get<std::string>();
    if (value.is_object()) return formatJsonObjectForConsole(value);
    if (value.is_array()) {
        std::string output;
        for (size_t i = 0; i < value.size(); ++i) {
            if (i > 0) output += "\n";
            output += "- " + formatJsonValueForConsole(value[i]);
        }
        return output;
    }
    return value.dump();
}

std::string formatToolMessageForConsole(const std::string& message) {
    constexpr std::string_view prefix = "[tool ";
    auto end = message.find("] ", prefix.size());
    if (!message.starts_with(prefix) || end == std::string::npos) return message;

    std::string content = message.substr(end + 2);
    try {
        content = formatJsonValueForConsole(nlohmann::json::parse(content));
    } catch (const nlohmann::json::parse_error&) {
    }

    return message.substr(0, end + 1) + "\n" + content;
}

} // namespace
// ---- consoleRenderEvents -----------------------------------------------------
// 从 Logger 事件缓冲区取出全部事件，输出到终端。

static void consoleRenderEvents(AFS_Logger& logger) {
    auto events = logger.poll();
    for (const auto& e : events) {
        switch (e.type) {
        case AgentEvent::Start:
            logger.output("");
            break;
        case AgentEvent::AssistantMessage:
            if (e.message_print) logger.output(*e.message_print);
            break;
        case AgentEvent::ToolResult:
            if (e.message_print) logger.output(formatToolMessageForConsole(*e.message_print));
            break;
        case AgentEvent::AssistantDelta:
            std::cout << e.text << std::flush;
            break;
        case AgentEvent::ReasoningMessage:
            if (!e.text.empty()) logger.output("\033[2m" + e.text + "\033[0m");
            break;
        case AgentEvent::ReasoningDelta:
            std::cout << "\033[2m" << e.text << "\033[0m" << std::flush;
            break;
        case AgentEvent::Error:
            AFS_LOG_ERROR(kRoleMain, "", e.text);
            break;
        case AgentEvent::Complete:
            if (!e.text.empty()) logger.output("\n" + e.text);
            break;
        }
    }
}

// ---- runConsole --------------------------------------------------------------
// 控制台模式：接收命令行 prompt，运行 Agent 并实时输出。

static int runConsole(const std::string& config_path, const std::string& prompt) {
    AFS_Logger::init();

    auto& config_manager = AFS_ConfigManager::instance();
    AFS_RegisterModelConfigSchemas();
    AFS_RegisterAgentConfigSchemas();
    AFS_RegisterPluginConfigSchemas();
    std::string config_error;
    if (!config_manager.loadFromFile(config_path, config_error)) {
        AFS_LOG_ERROR(kRoleMain, "", "无法加载配置: " + config_error);
        return 1;
    }
    auto models = AFS_LoadModelsConfig(config_manager);
    if (!models || models->llms.empty()) {
        AFS_LOG_ERROR(kRoleMain, "", "无法加载配置或无 LLM 模型");
        return 1;
    }

    auto plugin_config = AFS_LoadPluginRuntimeConfig(config_manager);
    AFS_PluginManager::instance()->loadFromDirectory(
        plugin_config ? std::filesystem::path(plugin_config->directory)
                      : AFS_DefaultPluginDirectory());
    auto agent = AFS_Agent::createMain();
    if (!agent) {
        AFS_LOG_ERROR(kRoleMain, "", "创建 Agent 失败");
        return 1;
    }

    agent->setModel(createModel(models->llms[0]));
    if (auto loop_config = AFS_LoadAgentLoopConfig(config_manager)) {
        agent->setLoopConfig(*loop_config);
    }
    agent->context().addMessage(AFS::UserMessage(prompt));

    AFS_Logger::instance().output(agent->context().messages().back().print());

    std::atomic<bool> done{false};
    std::string reply;
    std::thread runner([&]() {
        reply = agent->run();
        done.store(true);
    });

    while (!done.load()) {
        consoleRenderEvents(AFS_Logger::instance());
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
    runner.join();
    consoleRenderEvents(AFS_Logger::instance());

    if (reply.empty()) {
        AFS_LOG_ERROR(kRoleMain, "", "Agent 未返回有效回复");
        return 1;
    }
    return 0;
}

// ---- main --------------------------------------------------------------------

int main(int argc, char** argv) {
    if (argc == 1) {
        auto app = AFS_TuiApp::create(AFS_DefaultConfigPath());
        if (!app) return 1;
        app->run();
        return 0;
    }

    if (argc == 2) {
        auto app = AFS_TuiApp::create(argv[1]);
        if (!app) return 1;
        app->run();
        return 0;
    }

    return runConsole(argv[1], argv[2]);
}
