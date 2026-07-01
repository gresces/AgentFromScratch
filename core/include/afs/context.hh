#pragma once

#include "message.hh"

#include <cstddef>
#include <functional>
#include <string>
#include <vector>

namespace AFS {

// ---- Context -----------------------------------------------------------------
// Runtime context interface implemented by a context-type plugin.
// The host owns the object and passes it to the loop plugin for message updates.
class Context {
  public:
    virtual ~Context() = default;

    // ---- messages -----------------------------------------------------------
    virtual void addMessage(Message msg) = 0;
    virtual void addMessages(const std::vector<Message>& messages) = 0;
    virtual const std::vector<Message>& messages() const = 0;
    virtual std::size_t messageCount() const = 0;
    virtual std::vector<Message> buildRequest() const = 0;
    virtual std::string buildPrompt() const = 0;
    virtual void clear() = 0;

    // ---- token counting ----------------------------------------------------
    virtual void setTokenCounter(std::function<std::size_t(const std::string&)> counter) = 0;
    virtual void recomputeTokens(std::function<std::size_t(const std::string&)> counter) = 0;
    virtual std::size_t tokenCount() const = 0;
};

} // namespace AFS
