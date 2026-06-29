#pragma once

#include "tui/message/message.hh"

#include <ftxui/component/component.hpp>
#include <ftxui/dom/elements.hpp>

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
};

ftxui::InputOption AFS_TuiInputOption();
ftxui::Element AFS_TuiRenderStatus(const AFS_TuiStatusView& view);
ftxui::Element AFS_TuiRenderMessages(const std::vector<TuiMessage>& messages);
ftxui::Element AFS_TuiRenderInput(ftxui::Component input_component, bool shell_mode);
