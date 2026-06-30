#include "basic/models/model.hh"

#include <cpr/cpr.h>

// ---- helpers (file-local) ----------------------------------------------------

namespace {

std::optional<nlohmann::json> postJson(const std::string& url, const std::string& api_key,
                                       const nlohmann::json& body) {
    cpr::Response r = cpr::Post(
        cpr::Url{url},
        cpr::Header{{"Authorization", "Bearer " + api_key}, {"Content-Type", "application/json"}},
        cpr::Body{body.dump()});

    if (r.error) {
        return std::nullopt;
    }
    if (r.status_code < 200 || r.status_code >= 300) {
        return std::nullopt;
    }

    try {
        return nlohmann::json::parse(r.text);
    } catch (const nlohmann::json::parse_error&) {
        return std::nullopt;
    }
}

bool postJsonStream(const std::string& url, const std::string& api_key, const nlohmann::json& body,
                    const AFS_Model::ChatStreamCallback& on_chunk) {
    bool callback_ok = true;
    bool saw_done = false;

    cpr::Response r =
        cpr::Post(cpr::Url{url},
                  cpr::Header{{"Authorization", "Bearer " + api_key},
                              {"Content-Type", "application/json"},
                              {"Accept", "text/event-stream"}},
                  cpr::Body{body.dump()},
                  cpr::ServerSentEventCallback([&](cpr::ServerSentEvent&& event, intptr_t) {
                      if (event.data == "[DONE]") {
                          saw_done = true;
                          return true;
                      }

                      try {
                          callback_ok = on_chunk(nlohmann::json::parse(event.data));
                      } catch (const nlohmann::json::parse_error&) {
                          callback_ok = false;
                      }
                      return callback_ok;
                  }));

    if (r.error) return false;
    if (r.status_code < 200 || r.status_code >= 300) return false;
    return callback_ok && saw_done;
}

} // namespace

// ---- AFS_Model ---------------------------------------------------------------

AFS_Model::AFS_Model(std::string name) : name_(std::move(name)) {
}

// ---- AFS_Model_OpenAICompatible ----------------------------------------------

AFS_Model_OpenAICompatible::AFS_Model_OpenAICompatible(const AFS_ModelConfig& cfg)
    : AFS_Model(cfg.name), base_url_(cfg.base_url), api_key_(cfg.api_key), model_(cfg.model) {
}

std::string_view AFS_Model_OpenAICompatible::modelType() const {
    return "OpenAICompatible";
}

std::optional<nlohmann::json>
AFS_Model_OpenAICompatible::chatCompletion(const nlohmann::json& request) const {
    nlohmann::json body = request;
    if (!body.contains("model")) {
        body["model"] = model_;
    }
    return postJson(base_url_ + "/chat/completions", api_key_, body);
}

bool AFS_Model_OpenAICompatible::chatCompletionStream(const nlohmann::json& request,
                                                      const ChatStreamCallback& on_chunk) const {
    nlohmann::json body = request;
    if (!body.contains("model")) {
        body["model"] = model_;
    }
    body["stream"] = true;
    return postJsonStream(base_url_ + "/chat/completions", api_key_, body, on_chunk);
}

std::optional<nlohmann::json>
AFS_Model_OpenAICompatible::embedding(const nlohmann::json& request) const {
    nlohmann::json body = request;
    if (!body.contains("model")) {
        body["model"] = model_;
    }
    return postJson(base_url_ + "/embeddings", api_key_, body);
}

// ---- AFS_Model_DeepSeek ------------------------------------------------------

AFS_Model_DeepSeek::AFS_Model_DeepSeek(const AFS_ModelConfig& cfg)
    : AFS_Model_OpenAICompatible(cfg) {
}

std::string_view AFS_Model_DeepSeek::modelType() const {
    return "DeepSeek";
}

// ---- 工厂函数 ----------------------------------------------------------------

std::unique_ptr<AFS_Model> createModel(const AFS_ModelConfig& cfg) {
    if (cfg.base_url.find("api.deepseek.com") != std::string::npos) {
        return std::make_unique<AFS_Model_DeepSeek>(cfg);
    }
    return std::make_unique<AFS_Model_OpenAICompatible>(cfg);
}
