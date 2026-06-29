#pragma once

#include <spdlog/spdlog.h>

#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

// ---- AgentEvent -------------------------------------------------------------
// Agent 运行时事件，发布者 push 到 Logger 缓冲区，订阅者通过 poll() 取出。
struct AgentEvent {
    enum Type { Start, AssistantMessage, ToolResult, Error, Complete };

    Type type;
    std::string text;                                // Error / Complete 的文本
    std::optional<std::string> message_print;        // AssistantMessage / ToolResult 的 print() 结果
};

// ---- AFS_Logger --------------------------------------------------------------
// 全局单例日志与事件总线。程序启动时最先初始化。
// 系统日志走 spdlog（带时间戳+级别），用户输出走 stdout，
// Agent 运行时事件通过 publish*() 写入缓冲区，订阅者定时 poll() 取出。
class AFS_Logger {
  public:
    static void init();
    static AFS_Logger& instance();

    enum class Level { Trace, Debug, Info, Warning, Error, Critical };

    // 带文件位置的日志（系统错误/警告）
    void log(Level lvl, const char* file, int line, std::string_view agent, std::string_view role,
             std::string_view msg);

    // 不带文件位置的日志
    void log(Level lvl, std::string_view agent, std::string_view role, std::string_view msg);

    // 用户可见输出（stdout，无前缀）
    void output(std::string_view msg);

    // ---- 事件发布（由 Loop 调用，写入缓冲区）------------------------------
    void publishStart();
    void publishAssistantMessage(const std::string& msg_print);
    void publishToolResult(const std::string& msg_print);
    void publishError(const std::string& error);
    void publishComplete(const std::string& reply);

    // ---- 事件轮询（订阅者调用，取出并清空缓冲区）--------------------------
    std::vector<AgentEvent> poll();

  private:
    AFS_Logger();
    std::vector<AgentEvent> events_;
    std::mutex events_mutex_;
};

// ---- 日志宏（自动捕获文件/行号）----------------------------------------------
#define AFS_LOG_ERROR(agent, role, msg)                                                            \
    AFS_Logger::instance().log(AFS_Logger::Level::Error, __FILE__, __LINE__, agent, role, msg)
#define AFS_LOG_WARN(agent, role, msg)                                                             \
    AFS_Logger::instance().log(AFS_Logger::Level::Warning, __FILE__, __LINE__, agent, role, msg)
#define AFS_LOG_INFO(agent, role, msg)                                                             \
    AFS_Logger::instance().log(AFS_Logger::Level::Info, __FILE__, __LINE__, agent, role, msg)

// ---- 角色标签 ----------------------------------------------------------------
inline constexpr std::string_view kRoleSystem = "SYSTEM";
inline constexpr std::string_view kRoleUser = "USER";
inline constexpr std::string_view kRoleAssistant = "ASSISTANT";
inline constexpr std::string_view kRoleTool = "TOOL";
inline constexpr std::string_view kRoleLoop = "LOOP";
inline constexpr std::string_view kRolePlugin = "PLUGIN";
inline constexpr std::string_view kRoleMain = "MAIN";
