#include "config.hh"

#include <fstream>

// ---- from_json 实现 ----------------------------------------------------------

void from_json(const nlohmann::json& j, AFS_ModelConfig& cfg) {
    j.at("name").get_to(cfg.name);
    j.at("base_url").get_to(cfg.base_url);
    j.at("api_key").get_to(cfg.api_key);
    j.at("model").get_to(cfg.model);
}

void from_json(const nlohmann::json& j, AFS_ModelsConfig& models) {
    if (j.contains("llms")) {
        j.at("llms").get_to(models.llms);
    }
    if (j.contains("embeddings")) {
        j.at("embeddings").get_to(models.embeddings);
    }
}

// ---- AFS_Config --------------------------------------------------------------

std::optional<AFS_Config> AFS_Config::loadFromFile(const std::filesystem::path& path) {
    std::ifstream file(path);
    if (!file.is_open()) {
        return std::nullopt;
    }

    nlohmann::json root;
    try {
        file >> root;
    } catch (const nlohmann::json::parse_error&) {
        return std::nullopt;
    }

    AFS_Config config;
    if (root.contains("models")) {
        root.at("models").get_to(config.models_);
    }
    return config;
}
