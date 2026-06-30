#pragma once

#include "tui/message/message.hh"

#include <ftxui/component/component.hpp>
#include <ftxui/dom/elements.hpp>
#include <ftxui/screen/box.hpp>

#include <filesystem>
#include <array>
#include <string>
#include <vector>

struct AFS_TuiStatusView {
    bool esc_pending = false;
    bool agent_running = false;
    bool shell_running = false;
    bool shell_mode = false;
    int spinner_frame = 0;
    std::string model_name;
    std::string work_dir;
    std::size_t context_count = 0;
    std::size_t context_tokens = 0;
    std::size_t context_limit = 0;
};

struct AFS_TuiQuickIndexEntry {
    std::string label;
    int scroll_position = 0;
    ftxui::Box box;
};

enum class AFS_TuiSidebarMode {
    QuickIndex,
    Files,
};

struct AFS_TuiSidebarButton {
    AFS_TuiSidebarMode mode = AFS_TuiSidebarMode::QuickIndex;
    std::string label;
    ftxui::Box box;
};

struct AFS_TuiFileEntry {
    std::string label;
    std::filesystem::path path;
    int depth = 0;
    bool is_directory = false;
    bool is_expanded = false;
    bool is_expandable = false;
    ftxui::Box box;
};

struct AFS_TuiFileCandidates {
    bool active = false;
    int selected_index = 0;
    int visible_offset = 0;
    std::vector<std::string> labels;
};

using AFS_TuiSidebarButtons = std::array<AFS_TuiSidebarButton, 2>;

ftxui::InputOption AFS_TuiInputOption();
ftxui::Element AFS_TuiRenderStatus(const AFS_TuiStatusView& view);
ftxui::Element AFS_TuiRenderMessages(const std::vector<TuiMessage>& messages);
ftxui::Element AFS_TuiRenderQuickIndex(std::vector<AFS_TuiQuickIndexEntry>& entries);
ftxui::Element AFS_TuiRenderSidebar(AFS_TuiSidebarMode mode, AFS_TuiSidebarButtons& buttons,
                                    std::vector<AFS_TuiQuickIndexEntry>& quick_index_entries,
                                    std::vector<AFS_TuiFileEntry>& file_entries);
ftxui::Element AFS_TuiRenderInput(ftxui::Component input_component, bool shell_mode,
                                  const AFS_TuiFileCandidates& file_candidates);
