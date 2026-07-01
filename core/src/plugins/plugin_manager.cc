#include "plugins/plugin_manager.hh"
#include "basic/config/config.hh"
#include "basic/config/paths.hh"

#include <cctype>
#include <filesystem>
#include <dlfcn.h>
#include <nlohmann/json.hpp>

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

AFS_ConfigValueType parseFieldType(const std::string& value) {
    if (value == "number") return AFS_ConfigValueType::Number;
    if (value == "unsigned integer" || value == "unsigned")
        return AFS_ConfigValueType::UnsignedInteger;
    if (value == "boolean") return AFS_ConfigValueType::Boolean;
    return AFS_ConfigValueType::String;
}

std::vector<std::string> parsePath(const nlohmann::json& value) {
    std::vector<std::string> path;
    if (!value.is_array()) return path;
    for (const auto& segment : value) {
        if (segment.is_string()) path.push_back(segment.get<std::string>());
    }
    return path;
}

void registerPluginConfigSchemas(const AFS_LoadedPlugin& loaded) {
    if (!loaded.library()) return;
    auto* symbol =
        reinterpret_cast<AFS::ConfigSchemasFn>(dlsym(loaded.library(), "pluginConfigSchemas"));
    if (!symbol) return;

    const char* raw = symbol();
    if (!raw) return;

    nlohmann::json schemas_json;
    try {
        schemas_json = nlohmann::json::parse(raw);
    } catch (const nlohmann::json::exception&) {
        return;
    }
    if (!schemas_json.is_array()) return;

    auto& manager = AFS_ConfigManager::instance();
    for (const auto& item : schemas_json) {
        if (!item.is_object()) continue;
        AFS_ConfigSchema schema;
        schema.module = item.value("module", "");
        schema.path = parsePath(item.value("path", nlohmann::json::array()));
        schema.is_array = item.value("is_array", false);
        if (schema.module.empty() || schema.path.empty()) continue;

        const auto fields = item.value("fields", nlohmann::json::array());
        if (fields.is_array()) {
            for (const auto& field : fields) {
                if (!field.is_object() || !field.contains("name") || !field["name"].is_string()) {
                    continue;
                }
                schema.fields.push_back({
                    .name = field["name"].get<std::string>(),
                    .type = parseFieldType(field.value("type", "string")),
                    .required = field.value("required", false),
                    .sensitive = field.value("sensitive", false),
                    .default_value = field.value("default", nlohmann::json{}),
                });
            }
        }
        manager.registerSchema(std::move(schema));
    }
}

} // namespace

// ---- AFS_PluginRuntimeConfig --------------------------------------------------

AFS_ConfigSchema AFS_PluginRuntimeConfig::configSchema() {
    return {
        .module = "plugins",
        .path = {"plugins"},
        .is_array = false,
        .fields =
            {
                {"directory", AFS_ConfigValueType::String, false, false,
                 AFS_DefaultPluginDirectory().string()},
            },
    };
}

void from_json(const nlohmann::json& j, AFS_PluginRuntimeConfig& config) {
    config.directory = AFS_DefaultPluginDirectory().string();
    if (j.contains("directory") && j["directory"].is_string()) {
        config.directory = j["directory"].get<std::string>();
    }
}

void AFS_RegisterPluginConfigSchemas() {
    AFS_ConfigManager::instance().registerSchema(AFS_PluginRuntimeConfig::configSchema());
}

std::optional<AFS_PluginRuntimeConfig> AFS_LoadPluginRuntimeConfig(const AFS_Config& config) {
    try {
        const auto& root = config.root();
        if (!root.contains("plugins") || !root["plugins"].is_object()) {
            return AFS_PluginRuntimeConfig{AFS_DefaultPluginDirectory().string()};
        }
        return root.at("plugins").get<AFS_PluginRuntimeConfig>();
    } catch (const nlohmann::json::exception&) {
        return std::nullopt;
    }
}

std::optional<AFS_PluginRuntimeConfig>
AFS_LoadPluginRuntimeConfig(const AFS_ConfigManager& manager) {
    return AFS_LoadPluginRuntimeConfig(manager.config());
}

std::optional<AFS_PluginRuntimeConfig> AFS_LoadPluginRuntimeConfig() {
    return AFS_LoadPluginRuntimeConfig(AFS_ConfigManager::instance());
}

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

    if (!std::filesystem::exists(dir)) return;
    if (!std::filesystem::is_directory(dir)) return;

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
    registerPluginConfigSchemas(loaded);
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
