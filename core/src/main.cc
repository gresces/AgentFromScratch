#include "basic/log/logger.hh"
#include "agent/agent.hh"
#include "agent/loop/loop.hh"
#include "basic/config/config.hh"
#include "basic/models/model.hh"
#include "plugins/plugin_manager.hh"

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

    auto model = createModel(config->models().llms[0]);

    auto agent = AFS_Agent::createMain();
    if (!agent) {
        AFS_LOG_ERROR(kRoleMain, "", "创建 Agent 失败");
        return 1;
    }

    agent->context().addMessage(AFS::UserMessage(argv[2]));

    AFS_Logger::instance().output("");

    AFS_Loop loop;
    std::string reply = loop.run(*agent, *model);

    // 输出上下文消息
    for (const auto& msg : agent->context().messages()) {
        if (msg.role == AFS::Role::System || msg.role == AFS::Role::Developer) continue;
        AFS_Logger::instance().output(msg.print());
    }

    if (reply.empty()) {
        AFS_LOG_ERROR(kRoleMain, "", "Agent 未返回有效回复");
        return 1;
    }

    AFS_Logger::instance().output("\n" + reply);
    return 0;
}
