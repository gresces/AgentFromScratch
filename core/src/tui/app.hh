#pragma once

#include "tui/agent/bridge.hh"
#include "tui/layout/layout.hh"
#include <nlohmann/json.hpp>

#include <atomic>
#include <memory>
#include <filesystem>
#include <mutex>
#include <string>
#include <thread>
#include <set>
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
    // Load config.json into the configuration browser view.
    void refreshConfigView();

    // Save current TUI config state and reload model settings.
    void saveAndReloadConfig();

    // Apply a category/item navigation step in config mode.
    void moveConfigSelection(int category_delta, int item_delta);

    // Sync the edit input with the currently selected config option.
    void syncConfigEditBuffer();

    std::unique_ptr<AFS_TuiAgentBridge> agent_bridge_;

    // ---- UI state ------------------------------------------------------------
    std::vector<TuiMessage> messages_;
    std::vector<AFS_TuiQuickIndexEntry> quick_index_entries_;
    std::vector<AFS_TuiFileEntry> file_entries_;
    std::set<std::filesystem::path> expanded_file_dirs_;
    std::vector<AFS_TuiConfigCategory> config_categories_;
    AFS_TuiSidebarButtons sidebar_buttons_ = {{
        {AFS_TuiSidebarMode::QuickIndex, "Index", {}},
        {AFS_TuiSidebarMode::Files, "Files", {}},
    }};
    std::string input_;
    std::string file_candidate_query_;
    std::filesystem::path config_path_;
    nlohmann::json config_root_;
    std::string config_status_;
    std::string config_edit_value_;
    int file_candidate_index_ = 0;
    int scroll_position_ = 1000;
    int spinner_frame_ = 0;
    bool esc_pending_ = false;
    bool config_mode_ = false;
    double sidebar_ratio_ = 0.35;
    bool resizing_sidebar_ = false;
    bool shell_mode_ = false;
    bool follow_latest_ = true;
    AFS_TuiSidebarMode sidebar_mode_ = AFS_TuiSidebarMode::QuickIndex;
    int config_category_index_ = 0;
    int config_item_index_ = 0;
    ftxui::Box sidebar_splitter_box_;
    std::atomic<bool> shell_running_{false};
    std::atomic<bool> tui_running_{false};
    std::thread shell_thread_;
    std::mutex messages_mutex_;
};
