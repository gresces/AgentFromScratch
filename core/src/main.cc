#include "basic/log/logger.hh"
#include "agent/agent.hh"
#include "basic/config/config.hh"
#include "basic/models/model.hh"
#include "plugins/plugin_manager.hh"

#include <chrono>
#include <atomic>
#include <thread>

// ---- consolePoll -------------------------------------------------------------
// 从 Logger 事件缓冲区取出全部事件，渲染到终端。
// 后续 TUI / GUI 前端可在自己的渲染循环中定时调用 logger.poll()。

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

// ---- main --------------------------------------------------------------------

int main(int argc, char** argv) {
    // 程序启动：最先初始化日志
    AFS_Logger::init();

    if (argc < 3) {
        AFS_LOG_ERROR(kRoleMain, "", "用法: <config.json> <prompt>");
        return 1;
    }

    auto config = AFS_Config::loadFromFile(argv[1]);
    if (!config) {
        AFS_LOG_ERROR(kRoleMain, "", "无法加载配置文件");
        return 1;
    }
    if (config->models().llms.empty()) {
        AFS_LOG_ERROR(kRoleMain, "", "配置中没有 LLM 模型");
        return 1;
    }

    auto pm = AFS_PluginManager::instance();
    pm->loadFromDirectory("./plugins");

    auto agent = AFS_Agent::createMain();
    if (!agent) {
        AFS_LOG_ERROR(kRoleMain, "", "创建 Agent 失败");
        return 1;
    }

    agent->setModel(createModel(config->models().llms[0]));
    agent->context().addMessage(AFS::UserMessage(argv[2]));

    // 输出初始用户消息
    AFS_Logger::instance().output(agent->context().messages().back().print());

    // 后台线程运行 Agent Loop，主线程轮询 Logger 实时渲染事件
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

    // 排空残留事件
    consoleRenderEvents(AFS_Logger::instance());

    if (reply.empty()) {
        AFS_LOG_ERROR(kRoleMain, "", "Agent 未返回有效回复");
        return 1;
    }

    return 0;
}
