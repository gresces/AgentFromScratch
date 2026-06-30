#pragma once

#include "basic/config/config.hh"

#include <nlohmann/json.hpp>

#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <string_view>

// ---- AFS_Model ---------------------------------------------------------------
// 模型抽象基类：不可变，由配置创建，创建后内容不可修改。
// 所有成员数据在构造时深拷贝。
class AFS_Model {
  public:
    using ChatStreamCallback = std::function<bool(const nlohmann::json& chunk)>;

    virtual ~AFS_Model() = default;

    // ---- type identity -------------------------------------------------------
    virtual std::string_view modelType() const = 0;

    // ---- API 调用 ------------------------------------------------------------
    virtual std::optional<nlohmann::json> chatCompletion(const nlohmann::json& request) const = 0;

    virtual bool chatCompletionStream(const nlohmann::json& request,
                                      const ChatStreamCallback& on_chunk) const = 0;

    virtual std::optional<nlohmann::json> embedding(const nlohmann::json& request) const = 0;

    // ---- token counting ------------------------------------------------------
    // 估算字符串的 token 数量。子类可按模型特性覆盖。
    virtual std::size_t countTokens(const std::string& text) const;

    // ---- accessors -----------------------------------------------------------
    const std::string& name() const { return name_; }
    virtual std::string modelName() const = 0;

  protected:
    explicit AFS_Model(std::string name);
    AFS_Model(const AFS_Model&) = default;
    AFS_Model& operator=(const AFS_Model&) = default;

  private:
    std::string name_;
};

// ---- AFS_Model_OpenAICompatible ----------------------------------------------
// 兼容 OpenAI API 协议的模型实现。
// 支持 chat/completions 和 embeddings 两个端点。
class AFS_Model_OpenAICompatible : public AFS_Model {
  public:
    explicit AFS_Model_OpenAICompatible(const AFS_ModelConfig& cfg);

    std::string_view modelType() const override;
    std::string modelName() const override { return model_; }

    std::optional<nlohmann::json> chatCompletion(const nlohmann::json& request) const override;
    bool chatCompletionStream(const nlohmann::json& request,
                              const ChatStreamCallback& on_chunk) const override;
    std::optional<nlohmann::json> embedding(const nlohmann::json& request) const override;

  protected:
    // 子类可直接使用的字段：base_url、api_key、model。
    std::string base_url_;
    std::string api_key_;
    std::string model_;
};

// ---- AFS_Model_DeepSeek ------------------------------------------------------
// DeepSeek API 模型，继承自 OpenAI 兼容协议。
// 参考: https://api-docs.deepseek.com/
class AFS_Model_DeepSeek : public AFS_Model_OpenAICompatible {
  public:
    explicit AFS_Model_DeepSeek(const AFS_ModelConfig& cfg);

    std::string_view modelType() const override;
    std::size_t countTokens(const std::string& text) const override;
};

// ---- 工厂函数 ----------------------------------------------------------------
// 从 AFS_ModelConfig 创建模型实例，根据 base_url 自动识别协议类型。
std::unique_ptr<AFS_Model> createModel(const AFS_ModelConfig& cfg);
