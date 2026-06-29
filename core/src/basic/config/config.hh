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

// ---- nlohmann_json 反序列化适配 ----------------------------------------------
void from_json(const nlohmann::json& j, AFS_ModelConfig& cfg);
void from_json(const nlohmann::json& j, AFS_ModelsConfig& models);

// ---- AFS_Config --------------------------------------------------------------
// Agent 全局配置，从 JSON 文件加载，可由命令行参数覆盖。
// 不持有文件路径或原始 JSON，只保留解析后的结构化数据。
class AFS_Config {
  public:
    // ---- lifecycle ----------------------------------------------------------
    AFS_Config() = default;

    // 从 JSON 文件路径加载配置，失败时返回 std::nullopt。
    static std::optional<AFS_Config> loadFromFile(const std::filesystem::path& path);

    // ---- accessors ----------------------------------------------------------
    const AFS_ModelsConfig& models() const { return models_; }

  private:
    AFS_ModelsConfig models_;
};
