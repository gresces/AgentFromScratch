#pragma once

#include <nlohmann/json.hpp>

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

// ---- AFS_ModelConfig ---------------------------------------------------------
// 单个模型配置条目：可用于 LLM 或 embedding 模型。
struct AFS_ModelConfig {
    std::string name;     // 模型标识名
    std::string base_url; // API 基地址
    std::string api_key;  // API 密钥
    std::string model;    // 模型名称
};

// ---- AFS_ModelsConfig --------------------------------------------------------
// 模型分组配置：LLM 列表和 embedding 模型列表。
struct AFS_ModelsConfig {
    std::vector<AFS_ModelConfig> llms;
    std::vector<AFS_ModelConfig> embeddings;
};

// ---- AFS_TuiLayoutConfig -----------------------------------------------------
// TUI 布局偏好。比例值为 0..1，表示右侧区域占内容区宽度的比例。
struct AFS_TuiLayoutConfig {
    double sidebar_ratio = 0.35;
};

struct AFS_TuiConfig {
    AFS_TuiLayoutConfig layout;
};

// ---- nlohmann_json 反序列化适配 ----------------------------------------------
void from_json(const nlohmann::json& j, AFS_ModelConfig& cfg);
void from_json(const nlohmann::json& j, AFS_ModelsConfig& models);
void from_json(const nlohmann::json& j, AFS_TuiLayoutConfig& layout);
void from_json(const nlohmann::json& j, AFS_TuiConfig& tui);

// ---- AFS_Config --------------------------------------------------------------
// Agent 全局配置，从 JSON 文件加载，可由命令行参数覆盖。
// 不持有文件路径或原始 JSON，只保留解析后的结构化数据。
class AFS_Config {
  public:
    // ---- lifecycle ----------------------------------------------------------
    AFS_Config() = default;

    // 从 JSON 文件路径加载配置，失败时返回 std::nullopt。
    static std::optional<AFS_Config> loadFromFile(const std::filesystem::path& path);

    // 更新配置文件中的 TUI 右侧栏比例，保留其它 JSON 字段。
    static bool saveTuiSidebarRatio(const std::filesystem::path& path, double ratio);

    // ---- accessors ----------------------------------------------------------
    const AFS_ModelsConfig& models() const { return models_; }
    const AFS_TuiConfig& tui() const { return tui_; }

  private:
    AFS_ModelsConfig models_;
    AFS_TuiConfig tui_;
};
