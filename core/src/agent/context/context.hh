#pragma once

#include <afs/message.hh>

#include <cstddef>
#include <functional>
#include <string>
#include <vector>

class AFS_Model;

// ---- AFS_Context -------------------------------------------------------------
// Agent 上下文管理器。每个 AFS_Agent 持有一个实例。
// 负责累积对话历史、构建 LLM 请求提示、管理上下文窗口长度。
class AFS_Context {
  public:
    // 添加消息到上下文末尾。若已设置 token 计数器，自动累加 token。
    void addMessage(AFS::Message msg);

    // 批量添加消息（逐条调用 addMessage，自动累加 token）。
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

    // ---- token counting ----------------------------------------------------
    // 设置 token 计数回调。设置后 addMessage 自动调用来累加 token。
    // 回调签名：接受消息内容字符串，返回估算 token 数。
    void setTokenCounter(std::function<std::size_t(const std::string&)> counter);

    // 使用给定模型重新计算全部已有消息的 token 总数。
    void recomputeTokens(const AFS_Model& model);

    // 当前累计 token 数。
    std::size_t tokenCount() const { return token_count_; }

  private:
    std::vector<AFS::Message> messages_;
    std::size_t token_count_ = 0;
    std::function<std::size_t(const std::string&)> token_counter_;
};
