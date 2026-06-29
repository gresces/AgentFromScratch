#pragma once

#include <filesystem>

// Runtime paths derived from the user's configuration directory.
std::filesystem::path AFS_UserConfigDirectory();
std::filesystem::path AFS_DefaultConfigPath();
std::filesystem::path AFS_DefaultPluginDirectory();
