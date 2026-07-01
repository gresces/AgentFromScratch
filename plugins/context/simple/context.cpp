#include <afs.hh>

#include <cstdint>
#include <functional>
#include <string>
#include <utility>
#include <vector>

namespace {

// ---- DefaultContext ----------------------------------------------------------
class DefaultContext final : public AFS::Context {
  public:
    // ---- messages -----------------------------------------------------------
    void addMessage(AFS::Message msg) override {
        if (token_counter_) {
            token_count_ += token_counter_(msg.content);
        }
        messages_.push_back(std::move(msg));
    }

    void addMessages(const std::vector<AFS::Message>& messages) override {
        for (const auto& message : messages) {
            addMessage(message);
        }
    }

    const std::vector<AFS::Message>& messages() const override { return messages_; }
    std::size_t messageCount() const override { return messages_.size(); }
    std::vector<AFS::Message> buildRequest() const override { return messages_; }

    std::string buildPrompt() const override {
        std::string output;
        for (const auto& message : messages_) {
            output += message.print();
            output += "\n";
        }
        return output;
    }

    void clear() override {
        messages_.clear();
        token_count_ = 0;
    }

    // ---- token counting ----------------------------------------------------
    void setTokenCounter(std::function<std::size_t(const std::string&)> counter) override {
        token_counter_ = std::move(counter);
    }

    void recomputeTokens(std::function<std::size_t(const std::string&)> counter) override {
        token_count_ = 0;
        for (const auto& message : messages_) {
            token_count_ += counter(message.content);
        }
    }

    std::size_t tokenCount() const override { return token_count_; }

  private:
    std::vector<AFS::Message> messages_;
    std::size_t token_count_ = 0;
    std::function<std::size_t(const std::string&)> token_counter_;
};

// ---- ContextPlugin -----------------------------------------------------------
class ContextPluginSimple final : public AFS::Plugin {
  public:
    // ---- metadata -----------------------------------------------------------
    const char* name() const override { return "simple"; }
    AFS::PluginType type() const override { return AFS::PluginType::Context; }
    void start() override {}
    void stop() override {}

    // ---- factories ---------------------------------------------------------
    std::unique_ptr<AFS::Context> createContext() const override {
        return std::make_unique<DefaultContext>();
    }
};

} // namespace

// ---- plugin ABI --------------------------------------------------------------
AFS_PLUGIN_EXPORT std::uint32_t pluginAbiVersion() {
    return AFS::PluginAbiVersion;
}

AFS_PLUGIN_EXPORT AFS::Plugin* createPlugin() {
    return new ContextPluginSimple();
}

AFS_PLUGIN_EXPORT void destroyPlugin(AFS::Plugin* plugin) {
    delete plugin;
}
