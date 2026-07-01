#include "agent/agent.hh"
#include "plugins/plugin_manager.hh"

#include <afs/common.hh>

#include <algorithm>

// ---- 全局计数器：确保主 Agent 只创建一次 ----------------------------

namespace {

bool g_main_created = false;

} // namespace

// ---- AFS_Agent --------------------------------------------------------------

AFS_Agent::AFS_Agent(unsigned level) : level_(level), uuid_(AFS::uuid8()) {
    initDefaultContext();
}

AFS_Agent::AFS_Agent(unsigned level, const AFS_ToolRegistry& parent_tools)
    : level_(level), uuid_(AFS::uuid8()), tool_registry_(parent_tools) {
    initDefaultContext();
}

AFS_Agent::~AFS_Agent() {
    // 递归销毁子节点（触发其 tool_registry_ 清理）
    sub_agent_nodes_.clear();
    // 清理自己的工具注册表（释放插件函数对象）
    tool_registry_ = AFS_ToolRegistry{};
    // 卸载插件（此时所有函数对象已释放，安全 dlclose）
    auto pm = AFS_PluginManager::instance();
    for (const auto& [type, name] : loaded_plugins_) {
        pm->unloadPlugin(type, name);
    }
}

void AFS_Agent::initDefaultContext() {
    // 默认系统提示词，子 Agent 可根据任务覆盖
    context_.addMessage(
        AFS::SystemMessage("You are a helpful assistant. Use tools when appropriate."));
}

std::unique_ptr<AFS_Agent> AFS_Agent::createMain() {
    if (g_main_created) {
        return nullptr;
    }
    g_main_created = true;
    auto agent = std::unique_ptr<AFS_Agent>(new AFS_Agent(0));
    agent->registerTools();
    return agent;
}

AFS_Agent& AFS_Agent::genSubNode() {
    auto child = std::unique_ptr<AFS_Agent>(new AFS_Agent(level_ + 1, tool_registry_));
    AFS_Agent& ref = *child;
    sub_agent_nodes_.push_back(std::move(child));
    return ref;
}

void AFS_Agent::killSubNode(AFS_Agent& node) {
    auto it =
        std::find_if(sub_agent_nodes_.begin(), sub_agent_nodes_.end(),
                     [&node](const std::unique_ptr<AFS_Agent>& p) { return p.get() == &node; });

    if (it != sub_agent_nodes_.end()) {
        sub_agent_nodes_.erase(it);
    }
}

std::unique_ptr<AFS_Agent> AFS_Agent::swapWithChild(std::unique_ptr<AFS_Agent> self,
                                                    AFS_Agent& child) {
    auto it =
        std::find_if(self->sub_agent_nodes_.begin(), self->sub_agent_nodes_.end(),
                     [&child](const std::unique_ptr<AFS_Agent>& p) { return p.get() == &child; });

    if (it == self->sub_agent_nodes_.end()) {
        return self;
    }

    auto promoted = std::move(*it);
    self->sub_agent_nodes_.erase(it);

    std::swap(self->level_, promoted->level_);
    std::swap(self->tool_registry_, promoted->tool_registry_);
    std::swap(self->loaded_plugins_, promoted->loaded_plugins_);
    std::swap(self->context_, promoted->context_);

    promoted->sub_agent_nodes_.insert(promoted->sub_agent_nodes_.end(),
                                      std::make_move_iterator(self->sub_agent_nodes_.begin()),
                                      std::make_move_iterator(self->sub_agent_nodes_.end()));
    self->sub_agent_nodes_.clear();

    promoted->sub_agent_nodes_.push_back(std::move(self));

    fixSubtreeLevels(*promoted);

    return promoted;
}

// ---- tool management --------------------------------------------------------

namespace {

void registerCap(AFS_ToolRegistry& registry, const AFS::Plugin::ToolCap& cap) {
    AFS::ToolSpec spec{cap.name, cap.description, cap.input_schema};
    registry.registerTool(spec, [func = cap.func](const AFS_ToolCall& call) {
        AFS_ToolResult result;
        result.success = true;
        result.output = func(call.arguments);
        return result;
    });
}

} // namespace

void AFS_Agent::registerTools() {
    auto pm = AFS_PluginManager::instance();
    for (const auto& [type, name] : pm->loadedToolPlugins()) {
        for (const auto& cap : pm->toolCaps(type, name)) {
            registerCap(tool_registry_, cap);
        }
        loaded_plugins_.emplace_back(type, name);
    }

    // 注册完成后追加工具提示到上下文
    auto specs = tool_registry_.listSpecs();
    if (!specs.empty()) {
        std::string tool_prompt = "You have access to the following tools:\n";
        for (const auto& spec : specs) {
            tool_prompt += "- " + spec.name + ": " + spec.description;
            if (!spec.input_schema.empty()) {
                tool_prompt += " (input: " + spec.input_schema + ")";
            }
            tool_prompt += "\n";
        }
        context_.addMessage(AFS::SystemMessage(std::move(tool_prompt)));
    }
}

void AFS_Agent::loadExtraTool(AFS::PluginType type, const std::string& plugin_name) {
    auto pm = AFS_PluginManager::instance();
    pm->loadPlugin(type, plugin_name);
    loaded_plugins_.emplace_back(type, plugin_name);

    for (const auto& cap : pm->toolCaps(type, plugin_name)) {
        registerCap(tool_registry_, cap);
    }

    // 追加新加载工具的提示到上下文
    auto caps = pm->toolCaps(type, plugin_name);
    for (const auto& cap : caps) {
        std::string tool_prompt = "New tool available: " + cap.name + " - " + cap.description;
        if (!cap.input_schema.empty()) {
            tool_prompt += " (input: " + cap.input_schema + ")";
        }
        context_.addMessage(AFS::SystemMessage(std::move(tool_prompt)));
    }
}

void AFS_Agent::removeTool(const std::string& tool_name) {
    (void)tool_name; // 由 unloadPlugin 处理
}

// ---- printMain --------------------------------------------------------------

std::string AFS_Agent::printMain(const AFS_Agent& root) {
    std::string out;
    out += "[level=" + std::to_string(root.level_) +
           ", sub=" + std::to_string(root.sub_agent_nodes_.size()) + ", uuid=" + root.uuid_ + "]\n";

    const size_t count = root.sub_agent_nodes_.size();
    for (size_t i = 0; i < count; ++i) {
        printNode(out, *root.sub_agent_nodes_[i], "", i == count - 1);
    }
    return out;
}

void AFS_Agent::printNode(std::string& out, const AFS_Agent& node, const std::string& prefix,
                          bool is_last) {
    out += prefix;
    out += "+-- [level=" + std::to_string(node.level_) +
           ", sub=" + std::to_string(node.sub_agent_nodes_.size()) + ", uuid=" + node.uuid_ + "]\n";

    const std::string child_prefix = prefix + (is_last ? "    " : "|   ");
    const size_t count = node.sub_agent_nodes_.size();
    for (size_t i = 0; i < count; ++i) {
        printNode(out, *node.sub_agent_nodes_[i], child_prefix, i == count - 1);
    }
}

void AFS_Agent::fixSubtreeLevels(AFS_Agent& node) {
    for (auto& child : node.sub_agent_nodes_) {
        child->level_ = node.level_ + 1;
        fixSubtreeLevels(*child);
    }
}

// ---- execution -------------------------------------------------------------

void AFS_Agent::setModel(std::unique_ptr<AFS_Model> model) {
    model_ = std::move(model);
    context_.setTokenCounter(
        [raw = model_.get()](const std::string& text) { return raw->countTokens(text); });
    context_.recomputeTokens(*model_);
}

void AFS_Agent::setLoopConfig(AFS_AgentLoopConfig config) {
    loop_.setConfig(config);
}

std::string AFS_Agent::run() {
    if (!model_) return "";
    return loop_.run(context_, tool_registry_, *model_, uuid_);
}
