#include "agent/loop/loop.hh"
#include "basic/log/logger.hh"

#include <boost/sml.hpp>
namespace sml = boost::sml;

// ---- AFS_AgentLoopConfig ------------------------------------------------------

AFS_ConfigSchema AFS_AgentLoopConfig::configSchema() {
    return {
        .module = "agent.loop",
        .path = {"agent", "loop"},
        .is_array = false,
        .fields =
            {
                {"max_iterations", AFS_ConfigValueType::UnsignedInteger, true, false, 50},
            },
    };
}

void from_json(const nlohmann::json& j, AFS_AgentLoopConfig& config) {
    if (j.contains("max_iterations") && j["max_iterations"].is_number_unsigned()) {
        config.max_iterations = j["max_iterations"].get<unsigned>();
    }
}

void AFS_RegisterAgentConfigSchemas() {
    AFS_ConfigManager::instance().registerSchema(AFS_AgentLoopConfig::configSchema());
}

std::optional<AFS_AgentLoopConfig> AFS_LoadAgentLoopConfig(const AFS_Config& config) {
    try {
        const auto& root = config.root();
        if (!root.contains("agent") || !root["agent"].is_object()) return AFS_AgentLoopConfig{};
        const auto& agent = root["agent"];
        if (!agent.contains("loop") || !agent["loop"].is_object()) return AFS_AgentLoopConfig{};
        return agent.at("loop").get<AFS_AgentLoopConfig>();
    } catch (const nlohmann::json::exception&) {
        return std::nullopt;
    }
}

std::optional<AFS_AgentLoopConfig> AFS_LoadAgentLoopConfig(const AFS_ConfigManager& manager) {
    return AFS_LoadAgentLoopConfig(manager.config());
}

std::optional<AFS_AgentLoopConfig> AFS_LoadAgentLoopConfig() {
    return AFS_LoadAgentLoopConfig(AFS_ConfigManager::instance());
}

// ---- 状态机 -----------------------------------------------------------------

namespace {

struct start_event {};
struct llm_ok {
    nlohmann::json response;
};
struct tools_done {};
struct give_up {};

struct Idle {};
struct WaitingLLM {};
struct Executing {};
struct Finished {};

struct LoopData {
    nlohmann::json last_response;
    AFS_Loop::ParsedResponse last_parsed;
};

struct LoopTable {
    auto operator()() const {
        using namespace sml;
        // clang-format off
        return make_transition_table(
           *state<Idle>       + event<start_event> = state<WaitingLLM>,
            state<WaitingLLM> + event<llm_ok>     / [](LoopData& d, const llm_ok& e) {
                d.last_response = e.response;
            } = state<Executing>,
            state<Executing>  + event<tools_done>  = state<WaitingLLM>,
            state<WaitingLLM> + event<give_up>     = state<Finished>,
            state<Executing>  + event<give_up>     = state<Finished>
        );
        // clang-format on
    }
};

struct StreamToolCall {
    std::string id;
    std::string type;
    std::string name;
    std::string arguments;
};

struct StreamResult {
    bool ok = false;
    bool emitted = false;
    AFS_Loop::ParsedResponse parsed;
};

} // namespace

// ---- 请求构建 ---------------------------------------------------------------

static nlohmann::json buildRequest(const AFS_Context& ctx, const std::string& model_name,
                                   const AFS_ToolRegistry& registry) {
    nlohmann::json request;
    request["model"] = model_name;

    nlohmann::json messages = nlohmann::json::array();
    for (const auto& msg : ctx.messages()) {
        nlohmann::json j;
        switch (msg.role) {
        case AFS::Role::System:
            j["role"] = "system";
            break;
        case AFS::Role::User:
            j["role"] = "user";
            break;
        case AFS::Role::Assistant:
            j["role"] = "assistant";
            if (msg.tool_calls.has_value()) j["tool_calls"] = *msg.tool_calls;
            if (!msg.reasoning_content.empty()) j["reasoning_content"] = msg.reasoning_content;
            break;
        case AFS::Role::Tool:
            j["role"] = "tool";
            break;
        default:
            continue;
        }
        j["content"] = msg.content;
        if (msg.name) j["name"] = *msg.name;
        if (msg.tool_call_id) j["tool_call_id"] = *msg.tool_call_id;
        messages.push_back(std::move(j));
    }
    request["messages"] = std::move(messages);

    auto specs = registry.listSpecs();
    if (!specs.empty()) {
        nlohmann::json tools = nlohmann::json::array();
        for (const auto& spec : specs) {
            nlohmann::json tool;
            tool["type"] = "function";
            tool["function"]["name"] = spec.name;
            tool["function"]["description"] = spec.description;
            if (!spec.input_schema.empty()) {
                tool["function"]["parameters"] = nlohmann::json::parse(spec.input_schema);
            }
            tools.push_back(std::move(tool));
        }
        request["tools"] = std::move(tools);
    }
    return request;
}

// ---- 响应解析 ---------------------------------------------------------------

static AFS_Loop::ParsedResponse parseResponse(const nlohmann::json& response) {
    AFS_Loop::ParsedResponse pr;
    auto& choices = response.at("choices");
    if (choices.empty()) return pr;
    auto& msg = choices[0].at("message");
    if (msg.contains("content") && !msg["content"].is_null())
        pr.content = msg["content"].get<std::string>();
    if (msg.contains("tool_calls"))
        for (const auto& tc : msg["tool_calls"])
            pr.tool_calls.push_back(tc);
    if (msg.contains("reasoning_content") && !msg["reasoning_content"].is_null())
        pr.reasoning = msg["reasoning_content"].get<std::string>();
    return pr;
}

static void appendStringField(const nlohmann::json& object, const char* key, std::string& target) {
    if (object.contains(key) && !object[key].is_null()) {
        target += object[key].get<std::string>();
    }
}

static void assignStringField(const nlohmann::json& object, const char* key, std::string& target) {
    if (object.contains(key) && !object[key].is_null()) {
        target = object[key].get<std::string>();
    }
}

static nlohmann::json buildToolCallJson(const StreamToolCall& call) {
    nlohmann::json tool_call;
    tool_call["id"] = call.id;
    tool_call["type"] = call.type.empty() ? "function" : call.type;
    tool_call["function"] = {
        {"name", call.name},
        {"arguments", call.arguments},
    };
    return tool_call;
}

static void appendToolCallDelta(const nlohmann::json& delta,
                                std::vector<StreamToolCall>& tool_calls) {
    if (!delta.contains("tool_calls") || !delta["tool_calls"].is_array()) return;

    for (const auto& tool_delta : delta["tool_calls"]) {
        size_t index = tool_delta.value("index", tool_calls.size());
        if (index >= tool_calls.size()) tool_calls.resize(index + 1);

        auto& call = tool_calls[index];
        assignStringField(tool_delta, "id", call.id);
        assignStringField(tool_delta, "type", call.type);

        if (!tool_delta.contains("function") || !tool_delta["function"].is_object()) continue;
        const auto& function = tool_delta["function"];
        assignStringField(function, "name", call.name);
        appendStringField(function, "arguments", call.arguments);
    }
}

static StreamResult streamResponse(const AFS_Model& model, const nlohmann::json& request,
                                   AFS_Logger& logger) {
    StreamResult result;
    std::vector<StreamToolCall> tool_calls;

    result.ok = model.chatCompletionStream(request, [&](const nlohmann::json& chunk) {
        if (!chunk.contains("choices") || chunk["choices"].empty()) return true;

        const auto& choice = chunk["choices"][0];
        if (!choice.contains("delta") || !choice["delta"].is_object()) return true;

        const auto& delta = choice["delta"];
        std::string reasoning_delta;
        appendStringField(delta, "reasoning_content", reasoning_delta);
        appendStringField(delta, "reasoning", reasoning_delta);
        if (!reasoning_delta.empty()) {
            result.parsed.reasoning += reasoning_delta;
            result.emitted = true;
            logger.publishReasoningDelta(reasoning_delta);
        }

        std::string content_delta;
        appendStringField(delta, "content", content_delta);
        if (!content_delta.empty()) {
            result.parsed.content += content_delta;
            result.emitted = true;
            logger.publishAssistantDelta(content_delta);
        }

        appendToolCallDelta(delta, tool_calls);
        return true;
    });

    for (const auto& call : tool_calls) {
        if (!call.name.empty()) result.parsed.tool_calls.push_back(buildToolCallJson(call));
    }
    return result;
}

// ---- 工具执行 ---------------------------------------------------------------

static std::vector<AFS::Message> runTools(const std::vector<nlohmann::json>& tcs,
                                          AFS_ToolRegistry& reg) {
    std::vector<AFS::Message> out;
    for (const auto& tc : tcs) {
        std::string cid = tc.at("id").get<std::string>();
        auto& f = tc.at("function");
        std::string fname = f.at("name").get<std::string>();
        std::string fargs = f.at("arguments").get<std::string>();
        AFS_ToolCall call;
        call.name = fname;
        call.arguments = fargs;
        auto r = reg.execute(call);
        AFS::Message m;
        m.role = AFS::Role::Tool;
        m.tool_call_id = cid;
        m.content = r.success ? r.output : r.error;
        out.push_back(std::move(m));
    }
    return out;
}

// ---- 主循环 -----------------------------------------------------------------

std::string AFS_Loop::run(AFS_Context& context, AFS_ToolRegistry& tools, const AFS_Model& model,
                          const std::string& agent_uuid) {
    LoopData data;
    sml::sm<LoopTable> fsm{data};

    auto& logger = AFS_Logger::instance();
    logger.publishStart();
    fsm.process_event(start_event{});

    unsigned iter = 0;

    while (!fsm.is(sml::state<Finished>) && iter < config_.max_iterations) {

        if (fsm.is(sml::state<WaitingLLM>)) {
            auto req = buildRequest(context, model.modelName(), tools);
            auto streamed = streamResponse(model, req, logger);

            if (streamed.ok) {
                data.last_parsed = std::move(streamed.parsed);
            } else {
                if (streamed.emitted) {
                    logger.publishError("LLM 流式请求中断");
                    AFS_LOG_ERROR(agent_uuid, kRoleLoop, "LLM 流式请求中断");
                    fsm.process_event(give_up{});
                    break;
                }

                auto resp = model.chatCompletion(req);
                if (!resp) {
                    logger.publishError("LLM 请求失败");
                    AFS_LOG_ERROR(agent_uuid, kRoleLoop, "LLM 请求失败");
                    fsm.process_event(give_up{});
                    break;
                }
                data.last_parsed = parseResponse(*resp);
            }

            AFS::Message assistant_msg{
                .role = AFS::Role::Assistant,
                .content = data.last_parsed.content,
                .reasoning_content = data.last_parsed.reasoning,
            };
            if (!data.last_parsed.tool_calls.empty()) {
                assistant_msg.tool_calls = data.last_parsed.tool_calls;
            }

            if (!streamed.ok) {
                if (!data.last_parsed.reasoning.empty()) {
                    logger.publishReasoningMessage(data.last_parsed.reasoning);
                }
                AFS::Message display_msg = assistant_msg;
                display_msg.reasoning_content.clear();
                logger.publishAssistantMessage(display_msg.print());
            } else if (!data.last_parsed.tool_calls.empty()) {
                AFS::Message display_msg = assistant_msg;
                display_msg.content.clear();
                display_msg.reasoning_content.clear();
                logger.publishAssistantMessage(display_msg.print());
            }
            context.addMessage(std::move(assistant_msg));

            if (!data.last_parsed.hasToolCalls()) {
                return data.last_parsed.content;
            }
            fsm.process_event(llm_ok{nlohmann::json{}});
        }

        if (fsm.is(sml::state<Executing>)) {
            auto tmsgs = runTools(data.last_parsed.tool_calls, tools);
            for (auto& m : tmsgs) {
                logger.publishToolResult(m.print());
                context.addMessage(std::move(m));
            }
            ++iter;
            fsm.process_event(tools_done{});
        }
    }

    if (iter >= config_.max_iterations) {
        logger.publishError("达到最大迭代次数");
        AFS_LOG_ERROR(agent_uuid, kRoleLoop, "达到最大迭代次数");
    }

    for (auto it = context.messages().rbegin(); it != context.messages().rend(); ++it)
        if (it->role == AFS::Role::Assistant && !it->content.empty()) {
            logger.publishComplete(it->content);
            return it->content;
        }
    logger.publishComplete("");
    return "";
}
