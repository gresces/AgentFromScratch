#include "plugins/plugin_loader.hh"

#include <dlfcn.h>

#include <stdexcept>
#include <string>

// ---- helpers -----------------------------------------------------------------

namespace {

template <typename Fn> Fn loadSymbol(void* library, const char* name) {
    dlerror();
    void* symbol = dlsym(library, name);
    const char* error = dlerror();
    if (error != nullptr) {
        throw std::runtime_error(std::string("missing symbol: ") + name);
    }
    return reinterpret_cast<Fn>(symbol);
}

} // namespace

// ---- AFS_LoadedPlugin --------------------------------------------------------

void AFS_LoadedPlugin::reset() {
    if (plugin_ != nullptr && destroy_ != nullptr) {
        destroy_(plugin_);
        plugin_ = nullptr;
    }
    if (library_ != nullptr) {
        dlclose(library_);
        library_ = nullptr;
    }
}

// ---- AFS_PluginLoader --------------------------------------------------------

AFS_LoadedPlugin AFS_PluginLoader::load(const std::string& path) {
    void* library = dlopen(path.c_str(), RTLD_NOW);
    if (library == nullptr) {
        throw std::runtime_error(std::string("dlopen failed: ") + dlerror());
    }

    try {
        auto abiVersionFn = loadSymbol<AFS::AbiVersionFn>(library, "pluginAbiVersion");
        auto createFn = loadSymbol<AFS::CreateFn>(library, "createPlugin");
        auto destroyFn = loadSymbol<AFS::DestroyFn>(library, "destroyPlugin");

        if (abiVersionFn() != AFS::PluginAbiVersion) {
            throw std::runtime_error("plugin ABI version mismatch");
        }

        AFS::Plugin* plugin = createFn();
        if (plugin == nullptr) {
            throw std::runtime_error("createPlugin returned nullptr");
        }

        return AFS_LoadedPlugin(library, plugin, destroyFn);
    } catch (...) {
        dlclose(library);
        throw;
    }
}
