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
        // 仅保留 system/user/assistant/tool 角色
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
            continue; // 跳过 developer 等不支持的角色
        }
        j["content"] = msg.content;
        if (msg.name) j["name"] = *msg.name;
        if (msg.tool_call_id) j["tool_call_id"] = *msg.tool_call_id;
        messages.push_back(std::move(j));
    }
    request["messages"] = std::move(messages);

    // 附加工具定义
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

std::string AFS_Loop::run(AFS_Agent& agent, AFS_Model& model) {
    LoopData data;
    sml::sm<LoopTable> fsm{data};
    fsm.process_event(start_event{});

    unsigned iter = 0;
    std::string model_id(model.modelName());

    while (!fsm.is(sml::state<Finished>) && iter < kMaxIterations) {

        if (fsm.is(sml::state<WaitingLLM>)) {
            auto req = buildRequest(agent.context(), model_id, agent.toolRegistry());
            auto resp = model.chatCompletion(req);
            if (!resp) {
                AFS_LOG_ERROR(agent.uuid(), kRoleLoop, "LLM 请求失败");
                fsm.process_event(give_up{});
                break;
            }
            data.last_parsed = parseResponse(*resp);

            // 构建 Assistant 消息（含 tool_calls）
            AFS::Message assistant_msg;
            assistant_msg.role = AFS::Role::Assistant;
            assistant_msg.content = data.last_parsed.content;
            assistant_msg.reasoning_content = data.last_parsed.reasoning;
            if (!data.last_parsed.tool_calls.empty()) {
                assistant_msg.tool_calls = data.last_parsed.tool_calls;
            }
            agent.context().addMessage(std::move(assistant_msg));

            if (!data.last_parsed.hasToolCalls()) {
                return data.last_parsed.content;
            }
            fsm.process_event(llm_ok{*resp});
        }

        if (fsm.is(sml::state<Executing>)) {
            auto tmsgs = runTools(data.last_parsed.tool_calls, agent.toolRegistry());
            for (auto& m : tmsgs)
                agent.context().addMessage(std::move(m));
            // 工具结果已包含正确的 tool_call_id
            ++iter;
            fsm.process_event(tools_done{});
        }
    }

    if (iter >= kMaxIterations) AFS_LOG_ERROR(agent.uuid(), kRoleLoop, "达到最大迭代次数");

    for (auto it = agent.context().messages().rbegin(); it != agent.context().messages().rend();
         ++it)
        if (it->role == AFS::Role::Assistant && !it->content.empty()) return it->content;
    return "";
}
