#pragma once

#include "basic/config/config.hh"
#include "agent/context/context.hh"

#include <afs/loop.hh>

#include <nlohmann/json_fwd.hpp>

#include <optional>

using AFS_AgentLoopConfig = AFS::LoopConfig;
using AFS_Loop = AFS::Loop;

namespace AFS {

void from_json(const nlohmann::json& j, LoopConfig& config);

} // namespace AFS

void AFS_RegisterAgentConfigSchemas();
std::optional<AFS_AgentLoopConfig> AFS_LoadAgentLoopConfig(const AFS_Config& config);
std::optional<AFS_AgentLoopConfig> AFS_LoadAgentLoopConfig(const AFS_ConfigManager& manager);
std::optional<AFS_AgentLoopConfig> AFS_LoadAgentLoopConfig();
