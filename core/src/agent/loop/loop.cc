#include "agent/loop/loop.hh"
#include "basic/log/logger.hh"

#include <boost/sml.hpp>
namespace sml = boost::sml;

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

std::string AFS_Loop::run(AFS_Context& context, AFS_ToolRegistry& tools,
                          const AFS_Model& model, const std::string& agent_uuid) {
    LoopData data;
    sml::sm<LoopTable> fsm{data};

    auto& logger = AFS_Logger::instance();
    logger.publishStart();
    fsm.process_event(start_event{});

    unsigned iter = 0;

    while (!fsm.is(sml::state<Finished>) && iter < kMaxIterations) {

        if (fsm.is(sml::state<WaitingLLM>)) {
            auto req = buildRequest(context, model.modelName(), tools);
            auto resp = model.chatCompletion(req);
            if (!resp) {
                logger.publishError("LLM 请求失败");
                AFS_LOG_ERROR(agent_uuid, kRoleLoop, "LLM 请求失败");
                fsm.process_event(give_up{});
                break;
            }
            data.last_parsed = parseResponse(*resp);

            AFS::Message assistant_msg{
                .role = AFS::Role::Assistant,
                .content = data.last_parsed.content,
                .reasoning_content = data.last_parsed.reasoning,
            };
            if (!data.last_parsed.tool_calls.empty()) {
                assistant_msg.tool_calls = data.last_parsed.tool_calls;
            }
            logger.publishAssistantMessage(assistant_msg.print());
            context.addMessage(std::move(assistant_msg));

            if (!data.last_parsed.hasToolCalls()) {
                return data.last_parsed.content;
            }
            fsm.process_event(llm_ok{*resp});
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

    if (iter >= kMaxIterations) {
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
