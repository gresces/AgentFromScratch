#include "basic/models/model.hh"
#include "agent/context/context.hh"

// ---- AFS_Context -------------------------------------------------------------

void AFS_Context::addMessage(AFS::Message msg) {
    if (token_counter_) {
        token_count_ += token_counter_(msg.content);
    }
    messages_.push_back(std::move(msg));
}

void AFS_Context::addMessages(const std::vector<AFS::Message>& msgs) {
    for (const auto& msg : msgs) {
        addMessage(msg);
    }
}

std::vector<AFS::Message> AFS_Context::buildRequest() const {
    return messages_;
}

std::string AFS_Context::buildPrompt() const {
    std::string out;
    for (const auto& msg : messages_) {
        out += msg.print();
        out += "\n";
    }
    return out;
}

void AFS_Context::clear() {
    messages_.clear();
    token_count_ = 0;
}

void AFS_Context::setTokenCounter(std::function<std::size_t(const std::string&)> counter) {
    token_counter_ = std::move(counter);
}

void AFS_Context::recomputeTokens(const AFS_Model& model) {
    token_count_ = 0;
    for (const auto& msg : messages_) {
        token_count_ += model.countTokens(msg.content);
    }
}
