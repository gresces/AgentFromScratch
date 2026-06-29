#include "basic/log/logger.hh"
#include "agent/agent.hh"
#include "basic/config/config.hh"
#include "basic/config/paths.hh"
#include "basic/models/model.hh"
#include "plugins/plugin_manager.hh"
#include "tui/app.hh"

#include <atomic>
#include <chrono>
#include <thread>

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
        case AgentEvent::ToolResult:
            if (e.message_print) logger.output(*e.message_print);
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

    auto config = AFS_Config::loadFromFile(config_path);
    if (!config || config->models().llms.empty()) {
        AFS_LOG_ERROR(kRoleMain, "", "无法加载配置或无 LLM 模型");
        return 1;
    }

    AFS_PluginManager::instance()->loadFromDirectory(AFS_DefaultPluginDirectory());

    auto agent = AFS_Agent::createMain();
    if (!agent) {
        AFS_LOG_ERROR(kRoleMain, "", "创建 Agent 失败");
        return 1;
    }

    agent->setModel(createModel(config->models().llms[0]));
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
