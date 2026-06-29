#pragma once

#include "tui/agent/bridge.hh"
#include "tui/message/message.hh"

#include <atomic>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

// ---- AFS_TuiApp --------------------------------------------------------------
// FTXUI terminal frontend for the Agent core.
// Bottom input, message area above it, Agent Loop in a worker thread, Logger polling on UI refresh.
//
// Usage:
//   auto app = AFS_TuiApp::create("config.json");
//   if (app) app->run();
class AFS_TuiApp {
  public:
    static std::unique_ptr<AFS_TuiApp> create(const std::string& config_path);

    // Run the blocking FTXUI event loop. Esc requires a second press to exit.
    void run();

  private:
    AFS_TuiApp() = default;

    // Submit user input to the Agent bridge.
    void submit();

    // Execute shell input in the current working directory without touching Agent context.
    void submitShell();

    // Poll Logger buffered events and append them to messages_.
    void pollEvents();

    std::unique_ptr<AFS_TuiAgentBridge> agent_bridge_;

    // ---- UI state ------------------------------------------------------------
    std::vector<TuiMessage> messages_;
    std::string input_;
    int scroll_offset_ = 0;
    int spinner_frame_ = 0;
    bool esc_pending_ = false;
    bool shell_mode_ = false;
    std::atomic<bool> shell_running_{false};
    std::atomic<bool> tui_running_{false};
    std::thread shell_thread_;
    std::mutex messages_mutex_;
};
