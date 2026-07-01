#pragma once
#include "context.hh"
#include "loop.hh"

#include <cstdint>

#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace AFS {

// ---- ABI 版本 ---------------------------------------------------------------
inline constexpr std::uint32_t PluginAbiVersion = 3;

// ---- 导出宏 -----------------------------------------------------------------
#if defined(_WIN32)
#define AFS_PLUGIN_EXPORT extern "C" __declspec(dllexport)
#else
#define AFS_PLUGIN_EXPORT extern "C" __attribute__((visibility("default")))
#endif

// ---- PluginType --------------------------------------------------------------
enum class PluginType : std::uint32_t {
    Generic = 0,
    Tool = 1,
    Skill = 2,
    Context = 3,
    Loop = 4,
};

// ---- Plugin -----------------------------------------------------------------
// 所有插件的抽象基类。宿主通过虚函数调用，插件内部可使用完整 C++。
// 对象生命周期由插件自身的 create/destroy 函数管理。
class Plugin {
  public:
    virtual ~Plugin() = default;

    virtual const char* name() const = 0;
    virtual PluginType type() const = 0;
    virtual const char* version() const { return "1.0.0"; }

    virtual void start() = 0;
    virtual void stop() = 0;

    // ---- 工具插件能力（默认空列表） ------------------------------------------
    // 工具插件覆盖此方法返回其提供的工具能力。
    // 每个能力包含：名称、描述、输入 JSON Schema、执行函数。
    struct ToolCap {
        std::string name;
        std::string description;
        std::string input_schema;
        std::function<std::string(const std::string&)> func;
    };
    virtual std::vector<ToolCap> toolCapabilities() const { return {}; }

    // ---- 运行时插件能力（默认不提供） ----------------------------------------
    // context / loop 类型插件覆盖这些工厂方法（命名不影响发现）。
    virtual std::unique_ptr<Context> createContext() const { return nullptr; }
    virtual std::unique_ptr<Loop> createLoop() const { return nullptr; }
};

// ---- C ABI 导出签名 ---------------------------------------------------------
using AbiVersionFn = std::uint32_t (*)();
using CreateFn = Plugin* (*)();
using DestroyFn = void (*)(Plugin*);
// 可选导出：插件可提供 JSON 数组描述自身配置结构；宿主通过 dlsym 探测，
// 不要求旧插件实现，保持向后兼容。
// 建议 JSON 形状：
// [{"module":"plugin.tool.example","path":["plugins","tool","example"],"is_array":false,
//   "fields":[{"name":"enabled","type":"boolean","required":false}]}]
using ConfigSchemasFn = const char* (*)();

} // namespace AFS
