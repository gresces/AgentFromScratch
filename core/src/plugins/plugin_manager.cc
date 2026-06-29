#include "plugins/plugin_manager.hh"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <iostream>

// ---- helpers -----------------------------------------------------------------

namespace {

std::string capitalize(const std::string& s) {
    if (s.empty()) return s;
    std::string out = s;
    out[0] = static_cast<char>(std::toupper(out[0]));
    return out;
}

std::string typeName(AFS::PluginType type) {
    switch (type) {
    case AFS::PluginType::Tool:
        return "Tool";
    case AFS::PluginType::Skill:
        return "Skill";
    default:
        return "Generic";
    }
}

} // namespace

// ---- AFS_PluginManager -------------------------------------------------------

std::shared_ptr<AFS_PluginManager> AFS_PluginManager::instance() {
    static auto inst = std::shared_ptr<AFS_PluginManager>(new AFS_PluginManager());
    return inst;
}

std::string AFS_PluginManager::makeKey(AFS::PluginType type, const std::string& plugin_name) const {
    return typeName(type) + std::string("/") + plugin_name;
}

std::string AFS_PluginManager::pluginFileName(AFS::PluginType type,
                                              const std::string& plugin_name) const {
    // <Type>Plugin<Name>  如 ToolPluginCompute
    return typeName(type) + "Plugin" + capitalize(plugin_name);
}

std::string AFS_PluginManager::pluginDirName(AFS::PluginType type) const {
    switch (type) {
    case AFS::PluginType::Tool:
        return "tool";
    case AFS::PluginType::Skill:
        return "skill";
    default:
        return "generic";
    }
}

void AFS_PluginManager::loadFromDirectory(const std::filesystem::path& dir) {
    plugin_dir_ = dir;

    for (const auto& type_dir_entry : std::filesystem::directory_iterator(dir)) {
        if (!type_dir_entry.is_directory()) continue;

        std::string dir_name = type_dir_entry.path().filename().string();

        for (const auto& file_entry : std::filesystem::directory_iterator(type_dir_entry.path())) {
            if (!file_entry.is_regular_file()) continue;
            std::string filename = file_entry.path().filename().string();

            // 解析 "ToolPluginCompute" → type=Tool, name=compute
            std::string plugin_type_str;
            std::string plugin_name;
            for (const auto& t :
                 {AFS::PluginType::Tool, AFS::PluginType::Skill, AFS::PluginType::Generic}) {
                std::string prefix = typeName(t) + "Plugin";
                if (filename.starts_with(prefix)) {
                    plugin_type_str = typeName(t);
                    plugin_name = filename.substr(prefix.size());
                    break;
                }
            }
            if (plugin_name.empty()) continue;

            AFS::PluginType type;
            if (plugin_type_str == "Tool")
                type = AFS::PluginType::Tool;
            else if (plugin_type_str == "Skill")
                type = AFS::PluginType::Skill;
            else
                continue;

            loadPlugin(type, plugin_name);
        }
    }
}

void AFS_PluginManager::loadPlugin(AFS::PluginType type, const std::string& plugin_name) {
    std::string key = makeKey(type, plugin_name);

    auto it = plugins_.find(key);
    if (it != plugins_.end()) {
        it->second.ref_count++;
        return;
    }

    std::string filename = pluginFileName(type, plugin_name);
    auto path = plugin_dir_ / pluginDirName(type) / filename;

    AFS_LoadedPlugin loaded = AFS_PluginLoader::load(path.string());
    plugins_.emplace(key, PluginEntry{std::move(loaded), 1});
}

void AFS_PluginManager::unloadPlugin(AFS::PluginType type, const std::string& plugin_name) {
    std::string key = makeKey(type, plugin_name);
    auto it = plugins_.find(key);
    if (it == plugins_.end()) return;

    it->second.ref_count--;
    if (it->second.ref_count == 0) {
        plugins_.erase(it);
    }
}

std::vector<AFS::Plugin::ToolCap> AFS_PluginManager::allToolCaps() const {
    std::vector<AFS::Plugin::ToolCap> caps;
    for (const auto& [key, entry] : plugins_) {
        if (entry.loaded->type() != AFS::PluginType::Tool) continue;
        auto plugin_caps = entry.loaded->toolCapabilities();
        caps.insert(caps.end(), plugin_caps.begin(), plugin_caps.end());
    }
    return caps;
}

std::vector<AFS::Plugin::ToolCap>
AFS_PluginManager::toolCaps(AFS::PluginType type, const std::string& plugin_name) const {
    std::string key = makeKey(type, plugin_name);
    auto it = plugins_.find(key);
    if (it == plugins_.end()) return {};
    return it->second.loaded->toolCapabilities();
}

std::vector<std::pair<AFS::PluginType, std::string>> AFS_PluginManager::loadedToolPlugins() const {
    std::vector<std::pair<AFS::PluginType, std::string>> result;
    for (const auto& [key, entry] : plugins_) {
        if (entry.loaded->type() != AFS::PluginType::Tool) continue;
        // key format: "Tool/compute"
        auto slash = key.find('/');
        if (slash == std::string::npos) continue;
        result.emplace_back(AFS::PluginType::Tool, key.substr(slash + 1));
    }
    return result;
}
