#include "tui/app.hh"

#include "tui/input/input.hh"
#include "tui/layout/layout.hh"

#include <ftxui/component/component.hpp>
#include <ftxui/component/screen_interactive.hpp>
#include <ftxui/dom/elements.hpp>

#include <algorithm>
#include <array>
#include <chrono>
#include <cstdio>
#include <thread>
#include <sys/wait.h>

using namespace ftxui;

namespace {

constexpr int kSpinnerCount = 10;

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

// ---- lifecycle --------------------------------------------------------------

std::unique_ptr<AFS_TuiApp> AFS_TuiApp::create(const std::string& config_path) {
    auto bridge = AFS_TuiAgentBridge::create(config_path);
    if (!bridge) return nullptr;

    auto app = std::unique_ptr<AFS_TuiApp>(new AFS_TuiApp());
    app->agent_bridge_ = std::move(bridge);
    return app;
}

// ---- Agent interaction ------------------------------------------------------

void AFS_TuiApp::submit() {
    if (input_.empty() || agent_bridge_->running()) return;

    std::string user_input = std::move(input_);
    input_.clear();

    if (!agent_bridge_->submitUserMessage(user_input)) return;

    {
        std::lock_guard lock(messages_mutex_);
        messages_.push_back({TuiMessage::User, user_input, ""});
    }
}

void AFS_TuiApp::submitShell() {
    if (input_.empty() || shell_running_.load()) return;
    if (shell_thread_.joinable()) shell_thread_.join();

    std::string command = std::move(input_);
    input_.clear();

    {
        std::lock_guard lock(messages_mutex_);
        messages_.push_back({TuiMessage::Shell, "$ " + command, "command"});
        scroll_offset_ = static_cast<int>(messages_.size()) - 1;
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
            scroll_offset_ = static_cast<int>(messages_.size()) - 1;
        }
        shell_running_.store(false);
    });
}

// ---- Event polling ----------------------------------------------------------

void AFS_TuiApp::pollEvents() {
    auto new_messages = agent_bridge_->pollMessages();

    {
        std::lock_guard lock(messages_mutex_);
        bool was_at_bottom =
            (scroll_offset_ >= static_cast<int>(messages_.size()) - 1 || messages_.empty());

        messages_.insert(messages_.end(), new_messages.begin(), new_messages.end());

        if (was_at_bottom && !messages_.empty()) {
            scroll_offset_ = static_cast<int>(messages_.size()) - 1;
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
        });
    });

    auto input_opt = AFS_TuiInputOption();
    auto input_component = Input(&input_, "", input_opt);

    auto main_component = Renderer(input_component, [&] {
        int total = static_cast<int>(messages_.size());
        int max_offset = std::max(0, total - 1);
        if (scroll_offset_ < 0) scroll_offset_ = 0;
        if (scroll_offset_ > max_offset) scroll_offset_ = max_offset;

        return vbox({
            status_renderer->Render(),
            messages_renderer->Render() | focusPosition(0, scroll_offset_) | frame |
                vscroll_indicator | flex,
            separator(),
            AFS_TuiRenderInput(input_component, shell_mode_),
        });
    });

    main_component |= CatchEvent([this, &screen](Event event) -> bool {
        if (event == Event::Escape) {
            if (esc_pending_) {
                screen.Exit();
                return true;
            }
            esc_pending_ = true;
            return true;
        }

        if (AFS_TuiCancelsExitConfirmation(event)) esc_pending_ = false;

        if (AFS_TuiIsMultilineShortcut(event)) {
            input_ += '\n';
            return true;
        }

        if (event == Event::Tab) {
            shell_mode_ = !shell_mode_;
            return true;
        }

        if (event == Event::Return) {
            if (shell_mode_)
                submitShell();
            else
                submit();
            return true;
        }

        int total = static_cast<int>(messages_.size());
        if (AFS_TuiHandleScrollEvent(event, total, scroll_offset_)) return true;

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
