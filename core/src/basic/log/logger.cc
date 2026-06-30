#include "basic/log/logger.hh"

#include <spdlog/sinks/stdout_color_sinks.h>

#include <filesystem>

// ---- AFS_Logger --------------------------------------------------------------

void AFS_Logger::init() {
    instance();
}

AFS_Logger& AFS_Logger::instance() {
    static AFS_Logger inst;
    return inst;
}

AFS_Logger::AFS_Logger() {
    auto sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
    auto logger = std::make_shared<spdlog::logger>("afs", std::move(sink));
    logger->set_pattern("[%H:%M:%S] [%^%l%$] %v");
    spdlog::register_logger(logger);
    spdlog::set_default_logger(logger);
    spdlog::set_level(spdlog::level::info);

    // 用户可见输出 Logger（仅消息，无时间戳/级别前缀）
    auto out_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
    auto out_logger = std::make_shared<spdlog::logger>("afs_output", std::move(out_sink));
    out_logger->set_pattern("%v");
    spdlog::register_logger(out_logger);
}

void AFS_Logger::log(Level lvl, const char* file, int line, std::string_view agent,
                     std::string_view role, std::string_view msg) {
    auto l = [&] {
        switch (lvl) {
        case Level::Trace:
            return spdlog::level::trace;
        case Level::Debug:
            return spdlog::level::debug;
        case Level::Info:
            return spdlog::level::info;
        case Level::Warning:
            return spdlog::level::warn;
        case Level::Error:
            return spdlog::level::err;
        case Level::Critical:
            return spdlog::level::critical;
        }
        return spdlog::level::info;
    }();

    std::string filename = std::filesystem::path(file).filename().string();
    spdlog::log(l, "[{}][{}] {}:{} | {}", agent, role, filename, line, msg);
}

void AFS_Logger::log(Level lvl, std::string_view agent, std::string_view role,
                     std::string_view msg) {
    log(lvl, "", 0, agent, role, msg);
}

void AFS_Logger::output(std::string_view msg) {
    auto out = spdlog::get("afs_output");
    if (out) out->info("{}", msg);
}

// ---- 事件发布 --------------------------------------------------------------

void AFS_Logger::publishStart() {
    std::lock_guard lock(events_mutex_);
    events_.push_back({.type = AgentEvent::Start});
}

void AFS_Logger::publishAssistantMessage(const std::string& msg_print) {
    std::lock_guard lock(events_mutex_);
    events_.push_back({.type = AgentEvent::AssistantMessage, .message_print = msg_print});
}

void AFS_Logger::publishAssistantDelta(const std::string& delta) {
    std::lock_guard lock(events_mutex_);
    events_.push_back({.type = AgentEvent::AssistantDelta, .text = delta});
}

void AFS_Logger::publishReasoningMessage(const std::string& reasoning) {
    std::lock_guard lock(events_mutex_);
    events_.push_back({.type = AgentEvent::ReasoningMessage, .text = reasoning});
}

void AFS_Logger::publishReasoningDelta(const std::string& delta) {
    std::lock_guard lock(events_mutex_);
    events_.push_back({.type = AgentEvent::ReasoningDelta, .text = delta});
}

void AFS_Logger::publishToolResult(const std::string& msg_print) {
    std::lock_guard lock(events_mutex_);
    events_.push_back({.type = AgentEvent::ToolResult, .message_print = msg_print});
}

void AFS_Logger::publishError(const std::string& error) {
    std::lock_guard lock(events_mutex_);
    events_.push_back({.type = AgentEvent::Error, .text = error});
}

void AFS_Logger::publishComplete(const std::string& reply) {
    std::lock_guard lock(events_mutex_);
    events_.push_back({.type = AgentEvent::Complete, .text = reply});
}

// ---- 事件轮询 --------------------------------------------------------------

std::vector<AgentEvent> AFS_Logger::poll() {
    std::lock_guard lock(events_mutex_);
    return std::move(events_);
}
