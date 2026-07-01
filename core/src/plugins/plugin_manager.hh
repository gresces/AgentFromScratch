#pragma once

#include "afs/plugin.hh"
#include "plugins/plugin_loader.hh"
#include "basic/config/config.hh"

#include <filesystem>
#include <optional>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

// ---- AFS_PluginRuntimeConfig --------------------------------------------------
struct AFS_PluginRuntimeConfig {
    std::string directory;

    static AFS_ConfigSchema configSchema();
};

void from_json(const nlohmann::json& j, AFS_PluginRuntimeConfig& config);
void AFS_RegisterPluginConfigSchemas();
std::optional<AFS_PluginRuntimeConfig> AFS_LoadPluginRuntimeConfig(const AFS_Config& config);
std::optional<AFS_PluginRuntimeConfig>
AFS_LoadPluginRuntimeConfig(const AFS_ConfigManager& manager);
std::optional<AFS_PluginRuntimeConfig> AFS_LoadPluginRuntimeConfig();

// ---- AFS_PluginManager -------------------------------------------------------
// 全局插件管理器（单例），管理插件的加载、卸载和引用计数。
// 工具注册由 AFS_Agent 负责，插件管理器只管理插件生命周期。
class AFS_PluginManager {
  public:
    static std::shared_ptr<AFS_PluginManager> instance();

    AFS_PluginManager(const AFS_PluginManager&) = delete;
    AFS_PluginManager& operator=(const AFS_PluginManager&) = delete;

    // 从目录预加载所有插件（不注册工具，仅加载到内存）。
    void loadFromDirectory(const std::filesystem::path& dir);

    // 按类型和名称加载指定插件，引用计数 +1。
    // 文件路径: <dir>/<type>/<Type>Plugin<Name>
    // 如果已加载则仅递增引用计数。
    void loadPlugin(AFS::PluginType type, const std::string& plugin_name);

    // 释放插件引用，引用计数归零时自动 dlclose。
    void unloadPlugin(AFS::PluginType type, const std::string& plugin_name);

    // 获取所有已加载工具插件的能力函数列表。
    std::vector<AFS::Plugin::ToolCap> allToolCaps() const;

    // 获取指定类型和名称的插件能力（用于单个加载后查询）。
    std::vector<AFS::Plugin::ToolCap> toolCaps(AFS::PluginType type,
                                               const std::string& plugin_name) const;

    // 获取所有已加载工具插件的 (type, name) 列表。
    std::vector<std::pair<AFS::PluginType, std::string>> loadedToolPlugins() const;

  private:
    AFS_PluginManager() = default;

    struct PluginEntry {
        AFS_LoadedPlugin loaded;
        unsigned ref_count = 0;
    };

    // key: "<type>/<name>"
    std::string makeKey(AFS::PluginType type, const std::string& plugin_name) const;
    std::string pluginFileName(AFS::PluginType type, const std::string& plugin_name) const;
    std::string pluginDirName(AFS::PluginType type) const;

    std::filesystem::path plugin_dir_;
    std::unordered_map<std::string, PluginEntry> plugins_;
};
