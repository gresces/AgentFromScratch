#pragma once

#include "afs/plugin.hh"

#include <string>
#include <utility>

// ---- AFS_LoadedPlugin --------------------------------------------------------
// RAII 封装：持有动态库句柄、插件对象指针和销毁函数。
// 析构时先销毁插件对象，再关闭动态库（顺序不可颠倒）。
class AFS_LoadedPlugin {
  public:
    AFS_LoadedPlugin(void* library, AFS::Plugin* plugin, AFS::DestroyFn destroy)
        : library_(library), plugin_(plugin), destroy_(destroy) {}

    ~AFS_LoadedPlugin() { reset(); }

    AFS_LoadedPlugin(const AFS_LoadedPlugin&) = delete;
    AFS_LoadedPlugin& operator=(const AFS_LoadedPlugin&) = delete;

    AFS_LoadedPlugin(AFS_LoadedPlugin&& other) noexcept
        : library_(std::exchange(other.library_, nullptr)),
          plugin_(std::exchange(other.plugin_, nullptr)),
          destroy_(std::exchange(other.destroy_, nullptr)) {}

    AFS_LoadedPlugin& operator=(AFS_LoadedPlugin&& other) noexcept {
        if (this != &other) {
            reset();
            library_ = std::exchange(other.library_, nullptr);
            plugin_ = std::exchange(other.plugin_, nullptr);
            destroy_ = std::exchange(other.destroy_, nullptr);
        }
        return *this;
    }

    AFS::Plugin& get() const { return *plugin_; }
    AFS::Plugin* operator->() const { return plugin_; }
    void* library() const { return library_; }

  private:
    void reset();

    void* library_ = nullptr;
    AFS::Plugin* plugin_ = nullptr;
    AFS::DestroyFn destroy_ = nullptr;
};

// ---- AFS_PluginLoader --------------------------------------------------------
namespace AFS_PluginLoader {

AFS_LoadedPlugin load(const std::string& path);

} // namespace AFS_PluginLoader
