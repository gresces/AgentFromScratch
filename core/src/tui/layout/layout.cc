#include "tui/layout/layout.hh"

#include <algorithm>
#include <string>

using namespace ftxui;

namespace {

constexpr const char* kSpinnerFrames[] = {"⠋", "⠙", "⠹", "⠸", "⠼", "⠴", "⠦", "⠧", "⠇", "⠏"};
constexpr int kSpinnerCount = sizeof(kSpinnerFrames) / sizeof(kSpinnerFrames[0]);

Element roleHeader(const TuiMessage& message) {
    const char* role_name = "";
    Color role_color = Color::White;
    switch (message.role) {
    case TuiMessage::User:
        role_name = "user";
        role_color = Color::Green;
        break;
    case TuiMessage::Assistant:
        role_name = "assistant";
        role_color = Color::Cyan;
        break;
    case TuiMessage::Thinking:
        role_name = "thinking";
        role_color = Color::GrayDark;
        break;
    case TuiMessage::Tool:
        role_name = "tool";
        role_color = Color::Yellow;
        break;
    case TuiMessage::Shell:
        role_name = "shell";
        role_color = Color::Magenta;
        break;
    }

    Elements header_parts;
    header_parts.push_back(text("-- ") | dim);
    header_parts.push_back(text(role_name) | color(role_color) | bold);
    if (!message.detail.empty()) {
        header_parts.push_back(text(" " + message.detail) | dim);
    }
    header_parts.push_back(text(" "));
    header_parts.push_back(text(std::string(8, '-')) | dim | flex);
    return hbox(std::move(header_parts));
}

Element sidebarButton(AFS_TuiSidebarButton& button, AFS_TuiSidebarMode current_mode) {
    bool active = button.mode == current_mode;
    Element content = text(" " + button.label + " ");
    if (active) {
        content |= color(Color::Cyan) | bold | underlined;
    } else {
        content |= dim;
    }
    return content | reflect(button.box);
}

Element renderFileDirectory(std::vector<AFS_TuiFileEntry>& entries) {
    Elements lines;
    lines.push_back(text(" Directory") | bold);
    lines.push_back(separatorLight());

    if (entries.empty()) {
        lines.push_back(text(" Empty directory") | dim);
    } else {
        for (auto& entry : entries) {
            std::string indent(static_cast<std::size_t>(entry.depth) * 2, ' ');
            std::string marker = "[F] ";
            if (entry.is_directory && entry.is_expandable) {
                marker = entry.is_expanded ? "[-] " : "[+] ";
            } else if (entry.is_directory) {
                marker = "[D] ";
            }
            Element line = paragraph(indent + marker + entry.label) | reflect(entry.box);
            if (entry.is_directory) line |= color(Color::Cyan);
            lines.push_back(std::move(line));
        }
    }

    return vbox(std::move(lines)) | frame | vscroll_indicator | flex;
}

std::vector<std::string> splitLines(const std::string& value) {
    std::vector<std::string> lines;
    std::size_t start = 0;
    while (start <= value.size()) {
        std::size_t end = value.find('\n', start);
        if (end == std::string::npos) {
            lines.push_back(value.substr(start));
            break;
        }
        lines.push_back(value.substr(start, end - start));
        start = end + 1;
    }
    if (lines.empty()) lines.push_back("");
    return lines;
}

Element configCategories(const AFS_TuiConfigView& view) {
    Elements entries;
    for (std::size_t index = 0; index < view.categories.size(); ++index) {
        Element label = text(" " + view.categories[index].label + " ");
        if (static_cast<int>(index) == view.category_index) {
            label = label | color(Color::Black) | bgcolor(Color::Cyan) | bold;
        } else {
            label = label | color(Color::Cyan);
        }
        entries.push_back(std::move(label));
    }
    if (entries.empty()) entries.push_back(text(" config ") | color(Color::Cyan));
    return hbox(std::move(entries));
}

Element configItems(const AFS_TuiConfigView& view) {
    Elements lines;
    if (view.categories.empty() || view.categories[view.category_index].items.empty()) {
        lines.push_back(text(" No config items") | dim);
    } else {
        const auto& items = view.categories[view.category_index].items;
        for (std::size_t index = 0; index < items.size(); ++index) {
            Element line = paragraph(" " + items[index].label);
            if (static_cast<int>(index) == view.item_index) {
                line = line | color(Color::White) | bgcolor(Color::Blue) | bold;
            }
            lines.push_back(std::move(line));
        }
    }
    return vbox(std::move(lines)) | frame | vscroll_indicator | size(WIDTH, EQUAL, 30);
}

Element configDetail(const AFS_TuiConfigView& view, Component edit_component) {
    std::string detail = "No config item selected.";
    bool editable = false;
    if (!view.categories.empty() && !view.categories[view.category_index].items.empty()) {
        const auto& item = view.categories[view.category_index].items[view.item_index];
        detail = item.detail;
        editable = item.editable;
    }

    Elements lines;
    for (const auto& line : splitLines(detail)) {
        lines.push_back(paragraph(line));
    }
    if (editable) {
        lines.push_back(separatorLight());
        lines.push_back(hbox({
            text(" Value: "),
            edit_component->Render() | flex,
        }));
    }
    return vbox(std::move(lines)) | frame | vscroll_indicator | flex;
}

} // namespace

InputOption AFS_TuiInputOption(int* cursor_position) {
    InputOption input_opt;
    input_opt.multiline = true;
    input_opt.cursor_position = cursor_position;
    input_opt.transform = [](InputState state) {
        if (state.focused) {
            state.element |= bold;
        } else {
            state.element |= dim;
        }
        return state.element;
    };
    return input_opt;
}
Element AFS_TuiRenderStatus(const AFS_TuiStatusView& view) {
    if (view.esc_pending) {
        return text(" Press Esc again to exit") | color(Color::Yellow) | bold | hcenter;
    }

    Element left;
    if (view.agent_running || view.shell_running) {
        int frame = std::clamp(view.spinner_frame, 0, kSpinnerCount - 1);
        auto spinner = text(std::string(kSpinnerFrames[frame])) | color(Color::Cyan);
        auto label = text(view.shell_running ? " Shell running ..." : " Agent running ...");
        left = hbox({spinner, label});
    } else {
        left = text(view.shell_mode ? "  Shell ready" : "  Ready") | dim;
    }

    auto model = text(view.model_name) | dim;
    auto dir = text(view.work_dir) | dim;

    // Token count display
    std::string token_text;
    if (view.context_tokens > 0) {
        if (view.context_tokens >= 1'000'000) {
            token_text = std::to_string(view.context_tokens / 1'000'000) + "." +
                         std::to_string((view.context_tokens % 1'000'000) / 100'000) + "M";
        } else if (view.context_tokens >= 1'000) {
            token_text = std::to_string(view.context_tokens / 1'000) + "K";
        } else {
            token_text = std::to_string(view.context_tokens);
        }
        // Warning when >80% of limit
        if (view.context_limit > 0 && view.context_tokens > view.context_limit * 4 / 5) {
            token_text += "!";
        }
    }
    auto ctx_token = text(token_text) | dim;

    auto mode = text(view.shell_mode ? "SHELL" : "AGENT") |
                color(view.shell_mode ? Color::Magenta : Color::Cyan) | bold;
    auto separator = " | ";

    return hbox({
        left,
        filler(),
        mode,
        text(separator) | dim,
        model,
        text(separator) | dim,
        dir,
        text(separator) | dim,
        ctx_token,
    });
}
Element AFS_TuiRenderMessages(const std::vector<TuiMessage>& messages) {
    Elements lines;
    for (const auto& message : messages) {
        lines.push_back(roleHeader(message));
        Element content = paragraph(message.content);
        if (message.role == TuiMessage::Thinking) content |= dim;
        lines.push_back(std::move(content));
    }

    if (lines.empty()) {
        lines.push_back(text(" Type a message to start ...") | dim);
    }
    return vbox(std::move(lines));
}

Element AFS_TuiRenderQuickIndex(std::vector<AFS_TuiQuickIndexEntry>& entries) {
    Elements index_lines;
    index_lines.push_back(text(" Quick index") | bold);
    index_lines.push_back(separatorLight());

    if (entries.empty()) {
        index_lines.push_back(text(" No user messages") | dim);
    } else {
        for (auto& entry : entries) {
            index_lines.push_back(paragraph(entry.label) | reflect(entry.box));
        }
    }

    Element top = vbox(std::move(index_lines)) | frame | vscroll_indicator | flex;
    Element bottom = filler() | flex;
    return vbox({
        top,
        separatorLight(),
        bottom,
    });
}

Element AFS_TuiRenderSidebar(AFS_TuiSidebarMode mode, AFS_TuiSidebarButtons& buttons,
                             std::vector<AFS_TuiQuickIndexEntry>& quick_index_entries,
                             std::vector<AFS_TuiFileEntry>& file_entries) {
    Element tabs = hbox({
        sidebarButton(buttons[0], mode) | flex,
        sidebarButton(buttons[1], mode) | flex,
    });

    Element body = mode == AFS_TuiSidebarMode::QuickIndex
                       ? AFS_TuiRenderQuickIndex(quick_index_entries)
                       : renderFileDirectory(file_entries);

    return vbox({
        std::move(tabs),
        separatorLight(),
        std::move(body) | flex,
    });
}

Element renderFileCandidates(const AFS_TuiFileCandidates& file_candidates) {
    Elements lines;
    lines.push_back(text(" File candidates") | dim);
    if (file_candidates.labels.empty()) {
        lines.push_back(text(" No matching files") | dim);
    } else {
        const int begin = file_candidates.visible_offset;
        const int end = std::min<int>(begin + 6, static_cast<int>(file_candidates.labels.size()));
        for (int index = begin; index < end; ++index) {
            Element line = text(" " + file_candidates.labels[index]);
            if (index == file_candidates.selected_index) {
                line = line | color(Color::White) | bgcolor(Color::Blue) | bold;
            } else {
                line = line | color(Color::Cyan);
            }
            lines.push_back(std::move(line));
        }
    }
    return vbox(std::move(lines));
}

Element AFS_TuiRenderConfigMode(const AFS_TuiConfigView& view, Component edit_component) {
    Element header = hbox({
        text(" Config ") | bold | color(Color::Cyan),
        separator(),
        configCategories(view),
        filler(),
        text(view.esc_pending ? " Press Esc again to return "
                              : " Enter/Ctrl+S save, arrows select, Esc return ") |
            dim,
    });

    Element status =
        view.status.empty() ? text("") : text(" " + view.status) | color(Color::Yellow);

    return vbox({
        std::move(header),
        separatorLight(),
        hbox({
            configItems(view),
            separatorLight(),
            configDetail(view, edit_component),
        }) | flex,
        separatorLight(),
        std::move(status),
    });
}

Element AFS_TuiRenderInput(Component input_component, bool shell_mode,
                           const AFS_TuiFileCandidates& file_candidates) {
    Elements lines;
    lines.push_back(hbox({
        text(shell_mode ? " $ " : " > "),
        input_component->Render() | flex,
    }));
    if (file_candidates.active) {
        lines.push_back(renderFileCandidates(file_candidates));
    }
    return vbox(std::move(lines));
}
