#include "agent/loop/loop.hh"
#include "basic/config/paths.hh"

#include <nlohmann/json.hpp>

// ---- AFS::LoopConfig ---------------------------------------------------------

AFS_ConfigSchema AFS::LoopConfig::configSchema() {
    return {
        .module = "agent.loop",
        .path = {"agent", "loop"},
        .is_array = false,
        .fields =
            {
                {"max_iterations", AFS_ConfigValueType::UnsignedInteger, true, false, 50},
            },
    };
}

void AFS::from_json(const nlohmann::json& j, LoopConfig& config) {
    if (j.contains("max_iterations") && j["max_iterations"].is_number_unsigned()) {
        config.max_iterations = j["max_iterations"].get<unsigned>();
    }
}

// ---- agent loop config -------------------------------------------------------

void AFS_RegisterAgentConfigSchemas() {
    AFS_ConfigManager::instance().registerSchema(AFS::LoopConfig::configSchema());
}

std::optional<AFS_AgentLoopConfig> AFS_LoadAgentLoopConfig(const AFS_Config& config) {
    try {
        const auto& root = config.root();
        if (!root.contains("agent") || !root["agent"].is_object()) return AFS_AgentLoopConfig{};
        const auto& agent = root["agent"];
        if (!agent.contains("loop") || !agent["loop"].is_object()) return AFS_AgentLoopConfig{};
        return agent.at("loop").get<AFS_AgentLoopConfig>();
    } catch (const nlohmann::json::exception&) {
        return std::nullopt;
    }
}

std::optional<AFS_AgentLoopConfig> AFS_LoadAgentLoopConfig(const AFS_ConfigManager& manager) {
    return AFS_LoadAgentLoopConfig(manager.config());
}

std::optional<AFS_AgentLoopConfig> AFS_LoadAgentLoopConfig() {
    return AFS_LoadAgentLoopConfig(AFS_ConfigManager::instance());
}
