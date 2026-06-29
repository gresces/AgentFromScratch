#include "../../../core/include/afs.hh"

#include <cstdlib>
#include <cstring>
#include <string>

namespace {

// 从 JSON 中提取字符串值，跳过冒号后的空白
std::string extractString(const std::string& json, const std::string& key) {
    std::string search = "\"" + key + "\":";
    size_t pos = json.find(search);
    if (pos == std::string::npos) return "";
    pos += search.size();
    while (pos < json.size() && (json[pos] == ' ' || json[pos] == '\t'))
        ++pos;
    if (pos >= json.size() || json[pos] != '"') return "";
    ++pos; // 跳过引号
    size_t end = json.find('"', pos);
    if (end == std::string::npos) return "";
    return json.substr(pos, end - pos);
}

double extractNumber(const std::string& json, const std::string& key) {
    std::string search = "\"" + key + "\":";
    size_t pos = json.find(search);
    if (pos == std::string::npos) return 0;
    pos += search.size();
    while (pos < json.size() && (json[pos] == ' ' || json[pos] == '\t'))
        ++pos;
    char* end = nullptr;
    return std::strtod(json.c_str() + pos, &end);
}

} // namespace

class ComputePlugin final : public AFS::Plugin {
public:
    const char* name() const override { return "compute"; }
    AFS::PluginType type() const override { return AFS::PluginType::Tool; }
    void start() override {}
    void stop() override {}

    std::vector<ToolCap> toolCapabilities() const override {
        return {
            {"compute",
             "Binary arithmetic: add, sub, mul, div. Input: "
             "{\"op\":\"add\",\"a\":1,\"b\":2}",
             R"({"type":"object","properties":{"op":{"type":"string","enum":["add","sub","mul","div"]},"a":{"type":"number"},"b":{"type":"number"}},"required":["op","a","b"]})",
             [](const std::string& input) -> std::string {
                 std::string op = extractString(input, "op");
                 double a = extractNumber(input, "a");
                 double b = extractNumber(input, "b");

                 if (op == "add") {
                     return "{\"result\":" + std::to_string(a + b) + "}";
                 } else if (op == "sub") {
                     return "{\"result\":" + std::to_string(a - b) + "}";
                 } else if (op == "mul") {
                     return "{\"result\":" + std::to_string(a * b) + "}";
                 } else if (op == "div") {
                     if (b == 0) return R"({"error":"division by zero"})";
                     return "{\"result\":" + std::to_string(a / b) + "}";
                 }
                 return R"({"error":"unknown operation: )" + op + "\"}";
             }},
        };
    }
};

AFS_PLUGIN_EXPORT std::uint32_t pluginAbiVersion() {
    return AFS::PluginAbiVersion;
}
AFS_PLUGIN_EXPORT AFS::Plugin* createPlugin() { return new ComputePlugin(); }
AFS_PLUGIN_EXPORT void destroyPlugin(AFS::Plugin* p) { delete p; }
