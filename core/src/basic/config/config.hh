#pragma once

#include <nlohmann/json.hpp>

#include <filesystem>
#include <functional>
#include <string>
#include <unordered_map>
#include <vector>

// ---- AFS_ConfigValueType -----------------------------------------------------
enum class AFS_ConfigValueType {
    String,
    UnsignedInteger,
    Number,
    Boolean,
};

std::string AFS_ConfigValueTypeName(AFS_ConfigValueType type);

// ---- AFS_ConfigFieldSpec -----------------------------------------------------
struct AFS_ConfigFieldSpec {
    std::string name;
    AFS_ConfigValueType type = AFS_ConfigValueType::String;
    bool required = false;
    bool sensitive = false;
    nlohmann::json default_value;
};

// ---- AFS_ConfigSchema --------------------------------------------------------
struct AFS_ConfigSchema {
    std::string module;
    std::vector<std::string> path;
    bool is_array = false;
    std::vector<AFS_ConfigFieldSpec> fields;
};

// ---- AFS_Config --------------------------------------------------------------
// 动态配置值对象：只保存配置文件路径和原始 JSON，不包含任何业务模块 typed config。
class AFS_Config {
  public:
    bool loadFromFile(const std::filesystem::path& path, std::string& error);
    bool save(std::string& error) const;

    const std::filesystem::path& path() const { return path_; }
    const nlohmann::json& root() const { return root_; }
    const nlohmann::json* valueAt(const std::vector<std::string>& path) const;

    bool updateValue(const std::vector<std::string>& path, const nlohmann::json& value,
                     std::string& error);

  private:
    void ensureObjectRoot();

    std::filesystem::path path_;
    nlohmann::json root_ = nlohmann::json::object();
};

using AFS_ConfigApplyFn = std::function<bool(const AFS_Config& config, std::string& error)>;

// ---- AFS_ConfigUpdateResult --------------------------------------------------
struct AFS_ConfigUpdateResult {
    bool ok = false;
    std::string error;
    std::vector<std::string> affected_modules;
};

// ---- AFS_ConfigManager -------------------------------------------------------
// 动态配置管理器：维护配置类、模块 schema 注册表和可选应用回调。
// 不声明、不解析任何具体模块配置结构；models/tui/plugins 等模块自行声明类型并注册 schema。
class AFS_ConfigManager {
  public:
    static AFS_ConfigManager& instance();

    // ---- registration -------------------------------------------------------
    void registerSchema(AFS_ConfigSchema schema, AFS_ConfigApplyFn apply = {});
    const std::vector<AFS_ConfigSchema>& schemas() const { return schemas_; }

    // ---- file state ---------------------------------------------------------
    bool loadFromFile(const std::filesystem::path& path, std::string& error);
    bool save(std::string& error) const;

    // ---- config access ------------------------------------------------------
    const AFS_Config& config() const { return config_; }
    const std::filesystem::path& path() const { return config_.path(); }
    const nlohmann::json& root() const { return config_.root(); }
    const nlohmann::json* valueAt(const std::vector<std::string>& path) const {
        return config_.valueAt(path);
    }

    // ---- update/apply -------------------------------------------------------
    AFS_ConfigUpdateResult updateValue(const std::vector<std::string>& path,
                                       const nlohmann::json& value);
    bool applyModule(const std::string& module, std::string& error) const;
    bool applyModules(const std::vector<std::string>& modules, std::string& error) const;

  private:
    AFS_ConfigManager() = default;

    std::vector<std::string> affectedModulesForPath(const std::vector<std::string>& path) const;

    AFS_Config config_;
    std::vector<AFS_ConfigSchema> schemas_;
    std::unordered_map<std::string, AFS_ConfigApplyFn> appliers_;
};
