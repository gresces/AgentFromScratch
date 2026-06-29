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
    header_parts.push_back(text(std::string(200, '-')) | dim | flex);
    return hbox(std::move(header_parts));
}

} // namespace

InputOption AFS_TuiInputOption() {
    InputOption input_opt;
    input_opt.multiline = true;
    input_opt.transform = [](InputState state) {
        state.element |= dim;
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
    auto ctx_count = text("ctx " + std::to_string(view.context_count)) | dim;
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
        ctx_count,
    });
}

Element AFS_TuiRenderMessages(const std::vector<TuiMessage>& messages) {
    Elements lines;
    for (const auto& message : messages) {
        lines.push_back(roleHeader(message));
        lines.push_back(paragraph(message.content));
    }

    if (lines.empty()) {
        lines.push_back(text(" Type a message to start ...") | dim);
    }
    return vbox(std::move(lines));
}

Element AFS_TuiRenderInput(Component input_component, bool shell_mode) {
    return hbox({
        text(shell_mode ? " $ " : " > "),
        input_component->Render() | flex,
    });
}
