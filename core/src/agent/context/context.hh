#pragma once

#include <afs/message.hh>

#include <cstddef>
#include <string>
#include <vector>

// ---- AFS_Context -------------------------------------------------------------
// Agent 上下文管理器。每个 AFS_Agent 持有一个实例。
// 负责累积对话历史、构建 LLM 请求提示、管理上下文窗口长度。
class AFS_Context {
  public:
    // 添加消息到上下文末尾。
    void addMessage(AFS::Message msg);

    // 批量添加消息。
    void addMessages(const std::vector<AFS::Message>& msgs);

    // 获取所有消息。
    const std::vector<AFS::Message>& messages() const { return messages_; }

    // 消息数量。
    size_t messageCount() const { return messages_.size(); }

    // 构建 LLM 请求用的消息数组（当前直接返回全部消息）。
    std::vector<AFS::Message> buildRequest() const;

    // 构建可读的上下文摘要字符串。
    std::string buildPrompt() const;

    // 清空上下文。
    void clear();

  private:
    std::vector<AFS::Message> messages_;
};
