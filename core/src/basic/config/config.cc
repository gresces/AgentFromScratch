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

void from_json(const nlohmann::json& j, AFS_TuiLayoutConfig& layout) {
    if (j.contains("sidebar_ratio") && j["sidebar_ratio"].is_number()) {
        layout.sidebar_ratio = j["sidebar_ratio"].get<double>();
    }
}

void from_json(const nlohmann::json& j, AFS_TuiConfig& tui) {
    if (j.contains("layout")) {
        j.at("layout").get_to(tui.layout);
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
    try {
        if (root.contains("models") && !root["models"].is_object()) return std::nullopt;
        if (root.contains("tui") && !root["tui"].is_object()) return std::nullopt;
        if (root.contains("tui") && root["tui"].contains("layout") &&
            !root["tui"]["layout"].is_object())
            return std::nullopt;
        if (root.contains("models")) {
            root.at("models").get_to(config.models_);
        }
        if (root.contains("tui")) {
            root.at("tui").get_to(config.tui_);
        }
    } catch (const nlohmann::json::exception&) {
        return std::nullopt;
    }
    return config;
}

bool AFS_Config::saveTuiSidebarRatio(const std::filesystem::path& path, double ratio) {
    nlohmann::json root;
    {
        std::ifstream input(path);
        if (input.is_open()) {
            try {
                input >> root;
            } catch (const nlohmann::json::parse_error&) {
                return false;
            }
        }
    }

    root["tui"]["layout"]["sidebar_ratio"] = ratio;

    std::ofstream output(path);
    if (!output.is_open()) return false;
    output << root.dump(2) << '\n';
    return true;
}
