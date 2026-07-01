#pragma once

#include <nlohmann/json.hpp>

#include <cstddef>
#include <functional>
#include <optional>
#include <string>
#include <string_view>

namespace AFS {

// ---- Model -------------------------------------------------------------------
// Runtime model interface consumed by Loop plugins.
class Model {
  public:
    using ChatStreamCallback = std::function<bool(const nlohmann::json& chunk)>;

    virtual ~Model() = default;

    // ---- type identity ------------------------------------------------------
    virtual std::string_view modelType() const = 0;

    // ---- API calls ----------------------------------------------------------
    virtual std::optional<nlohmann::json> chatCompletion(const nlohmann::json& request) const = 0;
    virtual bool chatCompletionStream(const nlohmann::json& request,
                                      const ChatStreamCallback& on_chunk) const = 0;
    virtual std::optional<nlohmann::json> embedding(const nlohmann::json& request) const = 0;

    // ---- token counting -----------------------------------------------------
    virtual std::size_t countTokens(const std::string& text) const = 0;

    // ---- accessors ----------------------------------------------------------
    virtual const std::string& name() const = 0;
    virtual std::string modelName() const = 0;
};

} // namespace AFS
