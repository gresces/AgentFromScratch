#pragma once

#include <spdlog/spdlog.h>

#include <string>
#include <string_view>

// ---- AFS_Logger --------------------------------------------------------------
// 全局单例日志管理器。程序启动时最先初始化。
// 系统日志走 spdlog（带时间戳+级别），用户输出走 stdout。
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

  private:
    AFS_Logger();
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
