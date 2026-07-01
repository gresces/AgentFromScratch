#include "tui/app.hh"

#include "basic/config/config.hh"

#include "tui/input/input.hh"
#include "tui/layout/layout.hh"

#include <ftxui/component/component.hpp>
#include <ftxui/component/screen_interactive.hpp>
#include <ftxui/dom/elements.hpp>
#include <ftxui/screen/terminal.hpp>

#include <algorithm>
#include <cctype>
#include <array>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <cstdio>
#include <cstdlib>
#include <thread>
#include <set>
#include <string_view>
#include <nlohmann/json.hpp>
#include <sys/wait.h>

using namespace ftxui;

namespace {

constexpr int kSpinnerCount = 10;
constexpr int kScrollBottom = 1000;
constexpr int kFileCandidateVisibleLimit = 6;
constexpr int kSidebarThreshold = 130;
constexpr int kMessageMinWidth = 40;
constexpr int kSidebarMinWidth = 24;
constexpr double kMinSidebarRatio = 0.15;
constexpr double kMaxSidebarRatio = 0.75;

std::string shellQuote(const std::string& value) {
    std::string quoted = "'";
    for (char c : value) {
        if (c == '\'')
            quoted += "'\\''";
        else
            quoted += c;
    }
    quoted += "'";
    return quoted;
}

struct ShellResult {
    std::string output;
    int exit_code = 0;
};

bool canAppendToMessage(const TuiMessage& current, const TuiMessage& delta) {
    return delta.append && current.role == delta.role && current.detail == delta.detail;
}

void appendMessages(std::vector<TuiMessage>& messages, const std::vector<TuiMessage>& incoming) {
    for (const auto& message : incoming) {
        if (!messages.empty() && canAppendToMessage(messages.back(), message)) {
            messages.back().content += message.content;
            continue;
        }
        messages.push_back(message);
    }
}
std::string jsonScalarLabel(const nlohmann::json& value) {
    if (value.is_string()) return value.get<std::string>();
    if (value.is_number_integer()) return std::to_string(value.get<long long>());
    if (value.is_number_unsigned()) return std::to_string(value.get<unsigned long long>());
    if (value.is_number_float()) return value.dump();
    if (value.is_boolean()) return value.get<bool>() ? "true" : "false";
    if (value.is_null()) return "null";
    return value.is_array() ? "[" + std::to_string(value.size()) + "]"
                            : "{" + std::to_string(value.size()) + "}";
}

std::string configPathLabel(const std::vector<std::string>& path) {
    std::string label;
    for (const auto& segment : path) {
        if (!label.empty()) label += ".";
        label += segment;
    }
    return label;
}

bool isSensitiveConfigPath(const std::vector<std::string>& path) {
    if (path.empty()) return false;
    std::string lower;
    lower.reserve(path.back().size());
    for (char ch : path.back())
        lower.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(ch))));
    return lower.find("key") != std::string::npos || lower.find("token") != std::string::npos ||
           lower.find("secret") != std::string::npos || lower.find("password") != std::string::npos;
}

const nlohmann::json* configValueAt(const nlohmann::json& root,
                                    const std::vector<std::string>& path) {
    const nlohmann::json* current = &root;
    for (const auto& segment : path) {
        if (current->is_object()) {
            auto it = current->find(segment);
            if (it == current->end()) return nullptr;
            current = &(*it);
            continue;
        }
        if (current->is_array()) {
            std::size_t index = 0;
            try {
                index = static_cast<std::size_t>(std::stoull(segment));
            } catch (const std::exception&) {
                return nullptr;
            }
            if (index >= current->size()) return nullptr;
            current = &(*current)[index];
            continue;
        }
        return nullptr;
    }
    return current;
}

std::string configValueForInput(const nlohmann::json& root, const std::vector<std::string>& path,
                                std::string_view value_type, const std::string& fallback_value) {
    const nlohmann::json* value = configValueAt(root, path);
    if (!value) return fallback_value;
    if (value->is_string()) return value->get<std::string>();
    return jsonScalarLabel(*value);
}

std::string configDetailText(const nlohmann::json& root, const std::vector<std::string>& path,
                             std::string_view value_type, bool editable,
                             const nlohmann::json& default_value) {
    const nlohmann::json* value = configValueAt(root, path);
    std::string current = value ? jsonScalarLabel(*value) : "<missing>";
    if (isSensitiveConfigPath(path) && value && value->is_string()) current = "***";

    std::string detail = "Path: " + configPathLabel(path) + "\n";
    detail += "Type: " + std::string(value_type) + "\n";
    detail += "Current: " + current + "\n";
    if (!default_value.is_null()) detail += "Default: " + jsonScalarLabel(default_value) + "\n";
    detail += editable ? "Edit the value below, then press Ctrl+S to save and reload."
                       : "This row is read-only.";
    return detail;
}

AFS_TuiConfigItem makeConfigItem(const nlohmann::json& root, std::vector<std::string> path,
                                 std::string label, std::string value_type,
                                 const nlohmann::json& default_value) {
    return {
        .label = std::move(label),
        .detail = configDetailText(root, path, value_type, true, default_value),
        .value_type = std::move(value_type),
        .fallback_value = default_value.is_null() ? "" : jsonScalarLabel(default_value),
        .path = std::move(path),
        .editable = true,
    };
}

AFS_TuiConfigItem makeReadonlyConfigItem(std::string label, std::string detail) {
    return {
        .label = std::move(label),
        .detail = std::move(detail),
        .editable = false,
    };
}

std::string modelEntryName(const nlohmann::json& item, int index) {
    if (item.is_object() && item.contains("name") && item["name"].is_string()) {
        return item["name"].get<std::string>();
    }
    if (item.is_object() && item.contains("model") && item["model"].is_string()) {
        return item["model"].get<std::string>();
    }
    return "#" + std::to_string(index);
}

std::vector<AFS_TuiConfigCategory> buildConfigCategories(const nlohmann::json& root) {
    std::vector<AFS_TuiConfigCategory> categories;
    if (!root.is_object()) return categories;

    for (const auto& schema : AFS_ConfigManager::instance().schemas()) {
        AFS_TuiConfigCategory category;
        category.label = schema.module;

        const nlohmann::json* node = configValueAt(root, schema.path);
        if (schema.is_array) {
            if (!node || !node->is_array() || node->empty()) {
                category.items.push_back(makeReadonlyConfigItem(
                    "No entries", "No configured entries for " + schema.module + "."));
                categories.push_back(std::move(category));
                continue;
            }

            for (std::size_t index = 0; index < node->size(); ++index) {
                const nlohmann::json& item = (*node)[index];
                std::string index_segment = std::to_string(index);
                std::string prefix = "[" + index_segment + "] " +
                                     modelEntryName(item, static_cast<int>(index)) + " / ";
                for (const auto& field : schema.fields) {
                    auto path = schema.path;
                    path.push_back(index_segment);
                    path.push_back(field.name);
                    category.items.push_back(
                        makeConfigItem(root, std::move(path), prefix + field.name,
                                       AFS_ConfigValueTypeName(field.type), field.default_value));
                }
            }
            categories.push_back(std::move(category));
            continue;
        }

        for (const auto& field : schema.fields) {
            auto path = schema.path;
            path.push_back(field.name);
            category.items.push_back(makeConfigItem(root, std::move(path), field.name,
                                                    AFS_ConfigValueTypeName(field.type),
                                                    field.default_value));
        }
        categories.push_back(std::move(category));
    }
    return categories;
}

bool loadConfigJson(const std::filesystem::path& path, nlohmann::json& root, std::string& error) {
    std::ifstream input(path);
    if (!input.is_open()) {
        error = "无法打开配置文件: " + path.string();
        return false;
    }
    try {
        input >> root;
    } catch (const nlohmann::json::exception& ex) {
        error = std::string("配置 JSON 解析失败: ") + ex.what();
        return false;
    }
    return true;
}

bool hasFileCandidateTrigger(const std::string& input) {
    return !input.empty() && input.front() == '@';
}

std::string extractFilePathQuery(const std::string& input) {
    if (!hasFileCandidateTrigger(input)) return "";
    auto end = input.find_first_of(" \t\r\n", 1);
    if (end == std::string::npos) return input.substr(1);
    return input.substr(1, end - 1);
}

bool endsWithPathSeparator(const std::string& value) {
    return !value.empty() && (value.back() == '/' || value.back() == '\\');
}

std::filesystem::path expandLeadingHomePath(const std::string& query) {
    if (query == "~" || query.starts_with("~/")) {
        if (const char* home = std::getenv("HOME")) {
            std::filesystem::path suffix = query.size() > 2 ? query.substr(2) : "";
            return std::filesystem::path(home) / suffix;
        }
    }
    return query.empty() ? std::filesystem::path(".") : std::filesystem::path(query);
}

struct FileCandidate {
    std::string label;
    bool is_directory = false;
};

std::string fileCandidateLabel(const std::filesystem::path& path, bool is_directory) {
    std::error_code ec;
    std::string label = std::filesystem::relative(path, ec).string();
    if (ec || label.empty()) label = path.filename().string();
    if (is_directory) label += "/";
    return "@" + label;
}

int clampedCandidateIndex(int selected_index, int count) {
    if (count <= 0) return 0;
    return std::clamp(selected_index, 0, count - 1);
}

int visibleCandidateOffset(int selected_index, int count) {
    if (count <= kFileCandidateVisibleLimit) return 0;

    const int max_offset = count - kFileCandidateVisibleLimit;
    const int offset = selected_index - kFileCandidateVisibleLimit + 1;
    return std::clamp(offset, 0, max_offset);
}

std::string applyFileCandidate(const std::string& input, std::string_view label) {
    auto end = input.find_first_of(" \t\r\n", 1);
    if (end == std::string::npos) return std::string(label);
    return std::string(label) + input.substr(end);
}

void syncFileCandidateQuery(const std::string& input, std::string& query, int& selected_index) {
    std::string current_query = extractFilePathQuery(input);
    if (query == current_query) return;

    query = std::move(current_query);
    selected_index = 0;
}

AFS_TuiFileCandidates buildFileCandidates(const std::string& input, int selected_index) {
    AFS_TuiFileCandidates result;
    if (!hasFileCandidateTrigger(input)) return result;
    result.active = true;

    std::string query = extractFilePathQuery(input);
    std::filesystem::path typed_path = expandLeadingHomePath(query);

    std::error_code status_error;
    bool typed_is_directory = std::filesystem::is_directory(typed_path, status_error);
    std::filesystem::path root;
    std::string prefix;
    if (query.empty()) {
        root = ".";
    } else if (endsWithPathSeparator(query) || (!status_error && typed_is_directory)) {
        root = typed_path;
    } else {
        root = typed_path.parent_path();
        if (root.empty()) root = ".";
        prefix = typed_path.filename().string();
    }

    std::vector<FileCandidate> candidates;
    std::error_code iterate_error;
    std::filesystem::directory_iterator iterator(
        root, std::filesystem::directory_options::skip_permission_denied, iterate_error);
    std::filesystem::directory_iterator end;
    while (!iterate_error && iterator != end) {
        const auto& item = *iterator;
        std::string name = item.path().filename().string();
        if (!prefix.empty() && !name.starts_with(prefix)) {
            iterator.increment(iterate_error);
            continue;
        }

        std::error_code type_error;
        bool is_directory = item.is_directory(type_error);
        if (!type_error) {
            candidates.push_back({
                .label = fileCandidateLabel(root / item.path().filename(), is_directory),
                .is_directory = is_directory,
            });
        }
        iterator.increment(iterate_error);
    }

    std::sort(candidates.begin(), candidates.end(), [](const auto& left, const auto& right) {
        if (left.is_directory != right.is_directory) return left.is_directory;
        return left.label < right.label;
    });

    for (const auto& candidate : candidates) {
        result.labels.push_back(candidate.label);
    }
    result.selected_index =
        clampedCandidateIndex(selected_index, static_cast<int>(result.labels.size()));
    result.visible_offset =
        visibleCandidateOffset(result.selected_index, static_cast<int>(result.labels.size()));
    return result;
}

bool handleFileCandidateKey(Event event, std::string& input, std::string& query,
                            int& selected_index, int& cursor_position) {
    if (!hasFileCandidateTrigger(input)) return false;

    syncFileCandidateQuery(input, query, selected_index);
    AFS_TuiFileCandidates candidates = buildFileCandidates(input, selected_index);
    selected_index = candidates.selected_index;

    const bool navigates_candidates =
        event == Event::ArrowUp || event == Event::ArrowDown || event == Event::Tab;
    if (!navigates_candidates) return false;

    const int count = static_cast<int>(candidates.labels.size());
    if (event == Event::ArrowUp) {
        if (count > 0) selected_index = selected_index == 0 ? count - 1 : selected_index - 1;
        return true;
    }
    if (event == Event::ArrowDown) {
        if (count > 0) selected_index = (selected_index + 1) % count;
        return true;
    }
    if (count > 0) {
        input = applyFileCandidate(input, candidates.labels[selected_index]);
        cursor_position = static_cast<int>(input.size());
        syncFileCandidateQuery(input, query, selected_index);
    }
    return true;
}

std::string firstLine(std::string text) {
    auto pos = text.find('\n');
    if (pos != std::string::npos) text.resize(pos);
    return text;
}

std::vector<AFS_TuiQuickIndexEntry>
buildQuickIndexEntries(const std::vector<TuiMessage>& messages) {
    std::vector<AFS_TuiQuickIndexEntry> entries;
    int user_index = 1;
    int max_message_index = std::max(1, static_cast<int>(messages.size()) - 1);

    for (size_t i = 0; i < messages.size(); ++i) {
        if (messages[i].role != TuiMessage::User) continue;

        AFS_TuiQuickIndexEntry entry;
        entry.label = std::to_string(user_index++) + ". " + firstLine(messages[i].content);
        entry.scroll_position =
            static_cast<int>((static_cast<int>(i) * kScrollBottom) / max_message_index);
        entries.push_back(std::move(entry));
    }
    return entries;
}

std::filesystem::path currentDirectoryPath() {
    std::error_code error;
    auto path = std::filesystem::current_path(error);
    if (error) return ".";
    return path;
}

std::filesystem::path normalizedPath(const std::filesystem::path& path) {
    std::error_code error;
    auto absolute = std::filesystem::absolute(path, error);
    if (error) return path.lexically_normal();
    return absolute.lexically_normal();
}

std::vector<AFS_TuiFileEntry> readDirectoryEntries(const std::filesystem::path& root,
                                                   const std::set<std::filesystem::path>& expanded,
                                                   int depth) {
    std::vector<AFS_TuiFileEntry> entries;
    std::error_code error;
    std::filesystem::directory_iterator iterator(
        root, std::filesystem::directory_options::skip_permission_denied, error);
    if (error) return entries;

    std::filesystem::directory_iterator end;
    while (iterator != end) {
        const auto& item = *iterator;
        std::error_code status_error;
        bool is_directory = item.is_directory(status_error);
        if (!status_error) {
            std::error_code symlink_error;
            bool is_symlink = item.is_symlink(symlink_error);
            bool is_expandable = is_directory && !symlink_error && !is_symlink;
            std::filesystem::path entry_path = normalizedPath(item.path());
            entries.push_back({
                .label = item.path().filename().string(),
                .path = entry_path,
                .depth = depth,
                .is_directory = is_directory,
                .is_expanded = is_expandable && expanded.contains(entry_path),
                .is_expandable = is_expandable,
            });
        }

        iterator.increment(error);
        if (error) break;
    }

    std::sort(entries.begin(), entries.end(), [](const auto& left, const auto& right) {
        if (left.is_directory != right.is_directory) return left.is_directory;
        return left.label < right.label;
    });
    return entries;
}

void appendFileEntries(const std::filesystem::path& root,
                       const std::set<std::filesystem::path>& expanded,
                       std::vector<AFS_TuiFileEntry>& out, int depth) {
    auto entries = readDirectoryEntries(root, expanded, depth);
    for (auto& entry : entries) {
        bool recurse = entry.is_expanded;
        std::filesystem::path child_root = entry.path;
        out.push_back(std::move(entry));
        if (recurse) appendFileEntries(child_root, expanded, out, depth + 1);
    }
}

std::vector<AFS_TuiFileEntry> buildFileEntries(const std::filesystem::path& root,
                                               const std::set<std::filesystem::path>& expanded) {
    std::vector<AFS_TuiFileEntry> entries;
    appendFileEntries(root, expanded, entries, 0);
    return entries;
}

bool boxContains(const Box& box, int x, int y) {
    return x >= box.x_min && x <= box.x_max && y >= box.y_min && y <= box.y_max;
}

bool handleFileEntryClick(Event event, const std::vector<AFS_TuiFileEntry>& entries,
                          std::set<std::filesystem::path>& expanded_dirs,
                          std::vector<AFS_TuiFileEntry>& file_entries) {
    if (!event.is_mouse()) return false;
    auto mouse = event.mouse();
    if (mouse.button != Mouse::Left || mouse.motion != Mouse::Pressed) return false;

    for (const auto& entry : entries) {
        if (!entry.is_expandable || !boxContains(entry.box, mouse.x, mouse.y)) continue;
        if (entry.is_expanded) {
            expanded_dirs.erase(entry.path);
        } else {
            expanded_dirs.insert(entry.path);
        }
        file_entries = buildFileEntries(currentDirectoryPath(), expanded_dirs);
        return true;
    }
    return false;
}

bool handleFileEntryRightClick(Event event, const std::vector<AFS_TuiFileEntry>& entries,
                               std::string& input) {
    if (!event.is_mouse()) return false;
    auto mouse = event.mouse();
    if (mouse.button != Mouse::Right || mouse.motion != Mouse::Pressed) return false;

    for (const auto& entry : entries) {
        if (!boxContains(entry.box, mouse.x, mouse.y)) continue;
        std::error_code ec;
        std::string rel = std::filesystem::relative(entry.path, ec).string();
        if (ec) rel = entry.path.filename().string();
        input = "@" + rel;
        return true;
    }
    return false;
}

bool handleQuickIndexClick(Event event, const std::vector<AFS_TuiQuickIndexEntry>& entries,
                           int& scroll_position, bool& follow_latest) {
    if (!event.is_mouse()) return false;
    auto mouse = event.mouse();
    if (mouse.button != Mouse::Left || mouse.motion != Mouse::Pressed) return false;

    for (const auto& entry : entries) {
        if (!boxContains(entry.box, mouse.x, mouse.y)) continue;
        scroll_position = std::clamp(entry.scroll_position, 0, kScrollBottom);
        follow_latest = scroll_position == kScrollBottom;
        return true;
    }
    return false;
}

bool handleSidebarButtonClick(Event event, AFS_TuiSidebarButtons& buttons,
                              AFS_TuiSidebarMode& mode) {
    if (!event.is_mouse()) return false;
    auto mouse = event.mouse();
    if (mouse.button != Mouse::Left || mouse.motion != Mouse::Pressed) return false;

    for (const auto& button : buttons) {
        if (!boxContains(button.box, mouse.x, mouse.y)) continue;
        mode = button.mode;
        return true;
    }
    return false;
}

double clampSidebarRatio(double ratio) {
    return std::clamp(ratio, kMinSidebarRatio, kMaxSidebarRatio);
}

int sidebarWidthFor(int total_width, double ratio) {
    int max_sidebar_width = std::max(kSidebarMinWidth, total_width - kMessageMinWidth - 1);
    return std::clamp(static_cast<int>(total_width * clampSidebarRatio(ratio)), kSidebarMinWidth,
                      max_sidebar_width);
}

int messageRegionWidthFor(int total_width, int sidebar_width) {
    return std::max(1, total_width - sidebar_width - 1);
}

Element centerMessageArea(Element message_area, int region_width, int message_width) {
    if (region_width <= message_width) {
        return std::move(message_area) | size(WIDTH, EQUAL, region_width);
    }
    return hbox({
        filler(),
        std::move(message_area) | size(WIDTH, EQUAL, message_width),
        filler(),
    });
}

bool updateSidebarRatioFromMouse(int mouse_x, int total_width, double& sidebar_ratio) {
    if (total_width <= 0) return false;
    sidebar_ratio = clampSidebarRatio(static_cast<double>(total_width - mouse_x) / total_width);
    return true;
}

bool handleSidebarResize(Event event, int total_width, const Box& splitter_box,
                         const std::filesystem::path& config_path, double& sidebar_ratio,
                         bool& resizing_sidebar) {
    if (total_width <= kSidebarThreshold) return false;
    if (!event.is_mouse()) return false;
    auto mouse = event.mouse();

    if (mouse.button == Mouse::Left && mouse.motion == Mouse::Pressed &&
        boxContains(splitter_box, mouse.x, mouse.y)) {
        resizing_sidebar = true;
        updateSidebarRatioFromMouse(mouse.x, total_width, sidebar_ratio);
        return true;
    }

    if (!resizing_sidebar) return false;

    if (mouse.button == Mouse::Left && mouse.motion == Mouse::Released) {
        resizing_sidebar = false;
        updateSidebarRatioFromMouse(mouse.x, total_width, sidebar_ratio);
        AFS_TuiApp::saveSidebarRatio(config_path, sidebar_ratio);
        return true;
    }

    if (mouse.motion == Mouse::Moved || mouse.motion == Mouse::Pressed) {
        updateSidebarRatioFromMouse(mouse.x, total_width, sidebar_ratio);
        return true;
    }

    return true;
}

const AFS_TuiConfigItem* selectedConfigItem(const std::vector<AFS_TuiConfigCategory>& categories,
                                            int category_index, int item_index) {
    if (categories.empty()) return nullptr;
    if (category_index < 0 || category_index >= static_cast<int>(categories.size())) return nullptr;

    const auto& items = categories[category_index].items;
    if (items.empty()) return nullptr;
    if (item_index < 0 || item_index >= static_cast<int>(items.size())) return nullptr;
    return &items[item_index];
}

bool parseConfigEditValue(const AFS_TuiConfigItem& item, const std::string& text,
                          nlohmann::json& out, std::string& error) {
    try {
        if (item.value_type == "string") {
            out = text;
            return true;
        }
        if (item.value_type == "unsigned integer") {
            if (text.empty() || text.starts_with("-")) {
                error = "Expected an unsigned integer for " + configPathLabel(item.path);
                return false;
            }
            std::size_t consumed = 0;
            unsigned long long value = std::stoull(text, &consumed);
            if (consumed != text.size()) {
                error = "Expected an unsigned integer for " + configPathLabel(item.path);
                return false;
            }
            out = value;
            return true;
        }
        if (item.value_type == "number") {
            std::size_t consumed = 0;
            double value = std::stod(text, &consumed);
            if (consumed != text.size()) {
                error = "Expected a number for " + configPathLabel(item.path);
                return false;
            }
            out = value;
            return true;
        }
        if (item.value_type == "boolean") {
            if (text == "true" || text == "1") {
                out = true;
                return true;
            }
            if (text == "false" || text == "0") {
                out = false;
                return true;
            }
            error = "Expected a boolean for " + configPathLabel(item.path);
            return false;
        }
    } catch (const std::exception& ex) {
        error = "Invalid value for " + configPathLabel(item.path) + ": " + ex.what();
        return false;
    }

    error = "Unsupported config value type for " + configPathLabel(item.path);
    return false;
}

ShellResult executeShellCommand(const std::string& command) {
    ShellResult result;
    std::string wrapped = "/bin/bash -lc " + shellQuote(command) + " 2>&1";

    std::array<char, 4096> buffer{};
    FILE* pipe = popen(wrapped.c_str(), "r");
    if (!pipe) {
        result.exit_code = 127;
        result.output = "failed to start /bin/bash\n";
        return result;
    }

    while (fgets(buffer.data(), static_cast<int>(buffer.size()), pipe))
        result.output += buffer.data();

    int status = pclose(pipe);
    if (status == -1) {
        result.exit_code = 127;
    } else if (WIFEXITED(status)) {
        result.exit_code = WEXITSTATUS(status);
    } else if (WIFSIGNALED(status)) {
        result.exit_code = 128 + WTERMSIG(status);
    }
    return result;
}

} // namespace
// ---- AFS_TuiConfig -----------------------------------------------------------

AFS_ConfigSchema AFS_TuiLayoutConfig::configSchema() {
    return {
        .module = "tui.layout",
        .path = {"tui", "layout"},
        .is_array = false,
        .fields =
            {
                {
                    {"sidebar_ratio", AFS_ConfigValueType::Number, false, false, 0.35},
                },
            },
    };
}

void from_json(const nlohmann::json& j, AFS_TuiLayoutConfig& layout) {
    if (j.contains("sidebar_ratio") && j["sidebar_ratio"].is_number()) {
        layout.sidebar_ratio = j["sidebar_ratio"].get<double>();
    }
}

void from_json(const nlohmann::json& j, AFS_TuiConfig& tui) {
    if (j.contains("layout")) {
        j.at("layout").get_to(tui.layout);
    }
}

void AFS_TuiApp::registerConfigSchema() {
    AFS_ConfigManager::instance().registerSchema(AFS_TuiLayoutConfig::configSchema());
}

std::optional<AFS_TuiConfig> AFS_TuiApp::loadConfig(const AFS_Config& config_source) {
    AFS_TuiConfig config;
    try {
        const auto& root = config_source.root();
        if (root.contains("tui")) {
            if (!root["tui"].is_object()) return std::nullopt;
            root.at("tui").get_to(config);
        }
    } catch (const nlohmann::json::exception&) {
        return std::nullopt;
    }
    return config;
}

std::optional<AFS_TuiConfig> AFS_TuiApp::loadConfig(const AFS_ConfigManager& manager) {
    return loadConfig(manager.config());
}

bool AFS_TuiApp::saveSidebarRatio(const std::filesystem::path& path, double ratio) {
    auto& manager = AFS_ConfigManager::instance();
    std::string error;
    if (manager.path() != path && !manager.loadFromFile(path, error)) return false;
    auto result = manager.updateValue({"tui", "layout", "sidebar_ratio"}, ratio);
    if (!result.ok) return false;
    return manager.save(error);
}

// ---- lifecycle --------------------------------------------------------------

std::unique_ptr<AFS_TuiApp> AFS_TuiApp::create(const std::string& config_path) {
    AFS_TuiApp::registerConfigSchema();
    auto bridge = AFS_TuiAgentBridge::create(config_path);
    if (!bridge) return nullptr;

    auto app = std::unique_ptr<AFS_TuiApp>(new AFS_TuiApp());
    app->agent_bridge_ = std::move(bridge);
    app->config_path_ = config_path;
    if (auto config = AFS_TuiApp::loadConfig(AFS_ConfigManager::instance())) {
        app->sidebar_ratio_ = clampSidebarRatio(config->layout.sidebar_ratio);
    }
    app->file_entries_ = buildFileEntries(currentDirectoryPath(), app->expanded_file_dirs_);
    return app;
}

void AFS_TuiApp::refreshConfigView() {
    nlohmann::json root;
    std::string error;
    if (!loadConfigJson(config_path_, root, error)) {
        config_root_ = nlohmann::json::object();
        config_categories_.clear();
        config_category_index_ = 0;
        config_item_index_ = 0;
        config_edit_value_.clear();
        config_status_ = error;
        return;
    }

    config_root_ = std::move(root);
    config_categories_ = buildConfigCategories(config_root_);
    if (config_categories_.empty()) {
        config_category_index_ = 0;
        config_item_index_ = 0;
    } else {
        config_category_index_ =
            std::clamp(config_category_index_, 0, static_cast<int>(config_categories_.size()) - 1);
        const auto& items = config_categories_[config_category_index_].items;
        config_item_index_ =
            items.empty() ? 0
                          : std::clamp(config_item_index_, 0, static_cast<int>(items.size()) - 1);
    }
    syncConfigEditBuffer();
    config_status_ = "Config loaded: " + config_path_.string();
}

void AFS_TuiApp::syncConfigEditBuffer() {
    const AFS_TuiConfigItem* item =
        selectedConfigItem(config_categories_, config_category_index_, config_item_index_);
    if (!item || !item->editable) {
        config_edit_value_.clear();
        config_edit_cursor_position_ = 0;
        return;
    }
    config_edit_value_ =
        configValueForInput(config_root_, item->path, item->value_type, item->fallback_value);
    config_edit_cursor_position_ = static_cast<int>(config_edit_value_.size());
}

void AFS_TuiApp::saveAndReloadConfig() {
    const AFS_TuiConfigItem* item =
        selectedConfigItem(config_categories_, config_category_index_, config_item_index_);
    if (!item || !item->editable) {
        config_status_ = "Selected config row is read-only";
        return;
    }

    nlohmann::json parsed_value;
    std::string error;
    if (!parseConfigEditValue(*item, config_edit_value_, parsed_value, error)) {
        config_status_ = error;
        return;
    }

    auto& manager = AFS_ConfigManager::instance();
    if (!manager.loadFromFile(config_path_, error)) {
        config_status_ = error;
        return;
    }

    auto update = manager.updateValue(item->path, parsed_value);
    if (!update.ok) {
        config_status_ = update.error;
        return;
    }

    for (const auto& module : update.affected_modules) {
        if (module == "tui.layout" &&
            item->path == std::vector<std::string>{"tui", "layout", "sidebar_ratio"}) {
            sidebar_ratio_ =
                clampSidebarRatio(manager.root()["tui"]["layout"]["sidebar_ratio"].get<double>());
            manager.updateValue(item->path, sidebar_ratio_);
        }
    }

    if (!manager.save(error)) {
        config_status_ = error;
        return;
    }

    if (!manager.applyModules(update.affected_modules, error)) {
        config_status_ = "Config saved, but apply failed: " + error;
        return;
    }

    bool reload_model = false;
    for (const auto& module : update.affected_modules) {
        if (module == "models.llms" || module == "models.embeddings") reload_model = true;
    }
    if (reload_model && !agent_bridge_->reloadConfig(config_path_.string())) {
        config_status_ = "Config saved, but model reload failed";
        return;
    }

    refreshConfigView();
    config_status_ = reload_model ? "Config saved and model reloaded" : "Config saved";
}

void AFS_TuiApp::moveConfigSelection(int category_delta, int item_delta) {
    if (config_categories_.empty()) return;

    int category_count = static_cast<int>(config_categories_.size());
    int previous_category = config_category_index_;
    config_category_index_ =
        std::clamp(config_category_index_ + category_delta, 0, category_count - 1);

    int item_count = static_cast<int>(config_categories_[config_category_index_].items.size());
    if (item_count <= 0) {
        config_item_index_ = 0;
        syncConfigEditBuffer();
        return;
    }
    if (previous_category != config_category_index_) {
        config_item_index_ = std::clamp(config_item_index_, 0, item_count - 1);
    }
    config_item_index_ = std::clamp(config_item_index_ + item_delta, 0, item_count - 1);
    syncConfigEditBuffer();
}

// ---- Agent interaction ------------------------------------------------------

void AFS_TuiApp::submit() {
    if (input_.empty() || agent_bridge_->running()) return;
    std::string user_input = input_ == "." ? "keep going" : std::move(input_);
    input_.clear();
    input_cursor_position_ = 0;

    if (!agent_bridge_->submitUserMessage(user_input)) return;

    {
        std::lock_guard lock(messages_mutex_);
        messages_.push_back({TuiMessage::User, user_input, ""});
        scroll_position_ = kScrollBottom;
        follow_latest_ = true;
    }
}

void AFS_TuiApp::submitShell() {
    if (input_.empty() || shell_running_.load()) return;
    if (shell_thread_.joinable()) shell_thread_.join();
    std::string command = std::move(input_);
    input_.clear();
    input_cursor_position_ = 0;

    {
        std::lock_guard lock(messages_mutex_);
        messages_.push_back({TuiMessage::Shell, "$ " + command, "command"});
        scroll_position_ = kScrollBottom;
        follow_latest_ = true;
    }

    shell_running_.store(true);
    shell_thread_ = std::thread([this, command = std::move(command)] {
        ShellResult result = executeShellCommand(command);

        std::string content = result.output;
        if (content.empty()) content = "(no output)";
        {
            std::lock_guard lock(messages_mutex_);
            messages_.push_back(
                {TuiMessage::Shell, content, "exit_code=" + std::to_string(result.exit_code)});
            scroll_position_ = kScrollBottom;
            follow_latest_ = true;
        }
        shell_running_.store(false);
    });
}

// ---- Event polling ----------------------------------------------------------

void AFS_TuiApp::pollEvents() {
    auto new_messages = agent_bridge_->pollMessages();

    {
        std::lock_guard lock(messages_mutex_);
        appendMessages(messages_, new_messages);

        if (follow_latest_) {
            scroll_position_ = kScrollBottom;
        }
    }

    spinner_frame_ = (spinner_frame_ + 1) % kSpinnerCount;
}

// ---- TUI loop ---------------------------------------------------------------

void AFS_TuiApp::run() {
    auto screen = ScreenInteractive::FullscreenAlternateScreen();
    screen.TrackMouse(true);

    auto messages_renderer = Renderer([this] {
        std::lock_guard lock(messages_mutex_);
        return AFS_TuiRenderMessages(messages_);
    });

    auto status_renderer = Renderer([this] {
        return AFS_TuiRenderStatus({
            .esc_pending = esc_pending_,
            .agent_running = agent_bridge_->running(),
            .shell_running = shell_running_.load(),
            .shell_mode = shell_mode_,
            .spinner_frame = spinner_frame_,
            .model_name = agent_bridge_->modelName(),
            .work_dir = agent_bridge_->workDir(),
            .context_count = agent_bridge_->messageCount(),
            .context_tokens = agent_bridge_->tokenCount(),
            .context_limit = agent_bridge_->contextLimit(),
        });
    });
    auto input_component = Input(&input_, "", AFS_TuiInputOption(&input_cursor_position_));
    auto config_edit_component =
        Input(&config_edit_value_, "", AFS_TuiInputOption(&config_edit_cursor_position_));
    int active_input_index = 0;
    auto input_tabs = Container::Tab({input_component, config_edit_component}, &active_input_index);

    auto main_component = Renderer(input_tabs, [&] {
        active_input_index = config_mode_ ? 1 : 0;
        float scroll_position =
            follow_latest_ ? 1.0F : static_cast<float>(scroll_position_) / kScrollBottom;
        int terminal_width = std::max(1, Terminal::Size().dimx);
        bool show_sidebar = terminal_width > kSidebarThreshold;
        int sidebar_width = show_sidebar ? sidebarWidthFor(terminal_width, sidebar_ratio_) : 0;
        int message_region_width =
            show_sidebar ? messageRegionWidthFor(terminal_width, sidebar_width) : terminal_width;
        AFS_TuiFileCandidates file_candidates;
        if (shell_mode_) {
            file_candidate_query_.clear();
            file_candidate_index_ = 0;
        } else {
            syncFileCandidateQuery(input_, file_candidate_query_, file_candidate_index_);
            file_candidates = buildFileCandidates(input_, file_candidate_index_);
            file_candidate_index_ = file_candidates.selected_index;
        }

        {
            std::lock_guard lock(messages_mutex_);
            quick_index_entries_ = show_sidebar ? buildQuickIndexEntries(messages_)
                                                : std::vector<AFS_TuiQuickIndexEntry>{};
        }

        if (config_mode_) {
            return vbox({
                status_renderer->Render(),
                AFS_TuiRenderConfigMode(
                    {
                        .categories = config_categories_,
                        .category_index = config_category_index_,
                        .item_index = config_item_index_,
                        .status = config_status_,
                        .esc_pending = esc_pending_,
                    },
                    config_edit_component) |
                    flex,
            });
        }

        Element content_area =
            messages_renderer->Render() | size(WIDTH, EQUAL, message_region_width) |
            focusPositionRelative(0.0F, scroll_position) | frame | vscroll_indicator;
        if (show_sidebar) {
            Element splitter = separatorLight() | reflect(sidebar_splitter_box_);
            content_area = hbox({
                std::move(content_area) | size(WIDTH, EQUAL, message_region_width),
                std::move(splitter),
                AFS_TuiRenderSidebar(sidebar_mode_, sidebar_buttons_, quick_index_entries_,
                                     file_entries_) |
                    size(WIDTH, EQUAL, sidebar_width),
            });
        }

        return vbox({
            status_renderer->Render(),
            std::move(content_area) | flex,
            separator(),
            AFS_TuiRenderInput(input_component, shell_mode_, file_candidates),
        });
    });

    main_component |= CatchEvent([this, &screen](Event event) -> bool {
        if (event.is_character()) esc_pending_ = false;

        if (config_mode_) {
            if (event == Event::Escape) {
                if (esc_pending_) {
                    config_mode_ = false;
                    esc_pending_ = false;
                    screen.Post(Event::Custom);
                    return true;
                }
                esc_pending_ = true;
                return true;
            }
            if (event == Event::ArrowLeft) {
                esc_pending_ = false;
                moveConfigSelection(-1, 0);
                return true;
            }
            if (event == Event::ArrowRight) {
                esc_pending_ = false;
                moveConfigSelection(1, 0);
                return true;
            }
            if (event == Event::ArrowUp) {
                esc_pending_ = false;
                moveConfigSelection(0, -1);
                return true;
            }
            if (event == Event::ArrowDown) {
                esc_pending_ = false;
                moveConfigSelection(0, 1);
                return true;
            }
            if (event == Event::Return) {
                esc_pending_ = false;
                saveAndReloadConfig();
                return true;
            }
            if (event == Event::CtrlS) {
                esc_pending_ = false;
                saveAndReloadConfig();
                return true;
            }
            if (event == Event::CtrlP) {
                esc_pending_ = false;
                config_mode_ = false;
                screen.Post(Event::Custom);
                return true;
            }
            if (AFS_TuiHandleReadlineShortcut(event, config_edit_value_,
                                              config_edit_cursor_position_)) {
                esc_pending_ = false;
                return true;
            }
            return false;
        }
        if (!shell_mode_ && handleFileCandidateKey(event, input_, file_candidate_query_,
                                                   file_candidate_index_, input_cursor_position_)) {
            esc_pending_ = false;
            return true;
        }
        if (AFS_TuiHandleReadlineShortcut(event, input_, input_cursor_position_)) {
            esc_pending_ = false;
            return true;
        }

        if (auto key_action = AFS_TuiResolveKeyAction(event)) {
            if (key_action->cancels_exit_confirmation) esc_pending_ = false;

            switch (key_action->action) {
            case AFS_TuiKeyAction::BeginExit:
                if (esc_pending_) {
                    screen.Exit();
                    return true;
                }
                esc_pending_ = true;
                return true;
            case AFS_TuiKeyAction::InsertNewline:
                input_.insert(static_cast<std::size_t>(input_cursor_position_), "\n");
                ++input_cursor_position_;
                return true;
            case AFS_TuiKeyAction::ToggleShellMode:
                shell_mode_ = !shell_mode_;
                file_candidate_query_.clear();
                file_candidate_index_ = 0;
                return true;
            case AFS_TuiKeyAction::Submit:
                if (shell_mode_)
                    submitShell();
                else
                    submit();
                return true;
            case AFS_TuiKeyAction::OpenConfigMode:
                refreshConfigView();
                config_mode_ = true;
                shell_mode_ = false;
                esc_pending_ = false;
                file_candidate_query_.clear();
                file_candidate_index_ = 0;
                screen.Post(Event::Custom);
                return true;
            case AFS_TuiKeyAction::SaveConfig:
                saveAndReloadConfig();
                esc_pending_ = false;
                return true;
            case AFS_TuiKeyAction::Scroll: {
                int dimy = Terminal::Size().dimy;
                int frame_lines = std::max(1, dimy - 3);
                int content_lines;
                {
                    std::lock_guard lock(messages_mutex_);
                    content_lines =
                        std::max(frame_lines + 1, static_cast<int>(messages_.size()) * 3);
                }
                int page_units = 1000 * frame_lines / content_lines;
                int max_delta = std::max(1, page_units * 8 / 10);

                int delta;
                if (event == Event::PageUp) {
                    delta = -max_delta;
                } else if (event == Event::PageDown) {
                    delta = max_delta;
                } else if (event == Event::ArrowUp || event == Event::ArrowDown) {
                    int line_step = std::max(1, page_units / 6);
                    delta = event == Event::ArrowUp ? -line_step : line_step;
                } else {
                    delta = key_action->scroll_delta;
                    if (delta > 0)
                        delta = std::min(delta, max_delta);
                    else if (delta < 0)
                        delta = std::max(delta, -max_delta);
                }

                scroll_position_ = std::clamp(scroll_position_ + delta, 0, kScrollBottom);
                follow_latest_ = scroll_position_ == kScrollBottom;
                return true;
            }
            case AFS_TuiKeyAction::CancelExitConfirmation:
                return false;
            }
        }
        if (handleSidebarResize(event, Terminal::Size().dimx, sidebar_splitter_box_, config_path_,
                                sidebar_ratio_, resizing_sidebar_)) {
            return true;
        }

        auto previous_sidebar_mode = sidebar_mode_;
        if (handleSidebarButtonClick(event, sidebar_buttons_, sidebar_mode_)) {
            if (sidebar_mode_ == AFS_TuiSidebarMode::Files &&
                previous_sidebar_mode != AFS_TuiSidebarMode::Files) {
                file_entries_ = buildFileEntries(currentDirectoryPath(), expanded_file_dirs_);
            }
            return true;
        }

        if (sidebar_mode_ == AFS_TuiSidebarMode::Files &&
            handleFileEntryClick(event, file_entries_, expanded_file_dirs_, file_entries_)) {
            return true;
        }

        if (sidebar_mode_ == AFS_TuiSidebarMode::Files &&
            handleFileEntryRightClick(event, file_entries_, input_)) {
            input_cursor_position_ = static_cast<int>(input_.size());
            esc_pending_ = false;
            return true;
        }

        if (sidebar_mode_ == AFS_TuiSidebarMode::QuickIndex &&
            handleQuickIndexClick(event, quick_index_entries_, scroll_position_, follow_latest_)) {
            return true;
        }

        // 鼠标滚轮：动态计算步长
        if (event.is_mouse()) {
            auto& mouse = event.mouse();
            if (mouse.button == Mouse::WheelUp || mouse.button == Mouse::WheelDown) {
                int dimy = Terminal::Size().dimy;
                int frame_lines = std::max(1, dimy - 3);
                int content_lines;
                {
                    std::lock_guard lock(messages_mutex_);
                    content_lines =
                        std::max(frame_lines + 1, static_cast<int>(messages_.size()) * 3);
                }
                int page_units = 1000 * frame_lines / content_lines;
                int line_step = std::max(1, page_units / 6);
                int delta = mouse.button == Mouse::WheelUp ? -line_step : line_step;
                scroll_position_ = std::clamp(scroll_position_ + delta, 0, kScrollBottom);
                follow_latest_ = scroll_position_ == kScrollBottom;
                return true;
            }
        }

        if (AFS_TuiHandleScrollEvent(event, scroll_position_, follow_latest_)) return true;

        return false;
    });

    tui_running_.store(true);
    std::thread poller([&] {
        while (tui_running_.load()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            pollEvents();
            screen.Post(Event::Custom);
        }
    });

    screen.Loop(main_component);

    tui_running_.store(false);
    poller.join();
    if (shell_thread_.joinable()) shell_thread_.join();
}
