#include "basic/config/paths.hh"

#include <cstdlib>

namespace {

std::filesystem::path baseConfigDirectory() {
    if (const char* xdg_config_home = std::getenv("XDG_CONFIG_HOME")) {
        if (*xdg_config_home != '\0') return xdg_config_home;
    }

    if (const char* home = std::getenv("HOME")) {
        if (*home != '\0') return std::filesystem::path(home) / ".config";
    }

    return std::filesystem::current_path();
}

} // namespace

std::filesystem::path AFS_UserConfigDirectory() {
    return baseConfigDirectory() / "afs";
}

std::filesystem::path AFS_DefaultConfigPath() {
    return AFS_UserConfigDirectory() / "config.json";
}

std::filesystem::path AFS_DefaultPluginDirectory() {
    return AFS_UserConfigDirectory() / "plugins";
}
