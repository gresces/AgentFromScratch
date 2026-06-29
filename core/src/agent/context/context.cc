#include "agent/context/context.hh"

// ---- AFS_Context -------------------------------------------------------------

void AFS_Context::addMessage(AFS::Message msg) {
    messages_.push_back(std::move(msg));
}

void AFS_Context::addMessages(const std::vector<AFS::Message>& msgs) {
    messages_.insert(messages_.end(), msgs.begin(), msgs.end());
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
}
