#pragma once

#include <ftxui/component/event.hpp>

#include "tui/keymap/keymap.hh"

#include <optional>

// ---- AFS_TuiKeyActionEvent ---------------------------------------------------
struct AFS_TuiKeyActionEvent {
    AFS_TuiKeyAction action = AFS_TuiKeyAction::CancelExitConfirmation;
    int scroll_delta = 0;
    bool cancels_exit_confirmation = false;
};

// Resolve special keyboard input through AFS_TuiKeyBindings.
std::optional<AFS_TuiKeyActionEvent> AFS_TuiResolveKeyAction(const ftxui::Event& event);

// Return true for modified Enter key sequences that terminals send when
// Ctrl+Enter or Shift+Enter are distinguishable from plain Enter.
bool AFS_TuiIsMultilineShortcut(const ftxui::Event& event);

// Return true when an event should cancel the "press Esc again to exit" state.
bool AFS_TuiCancelsExitConfirmation(const ftxui::Event& event);

// Apply mouse/keyboard scrolling. Returns true when the event was consumed.
// scroll_position is a 0..1000 relative position for the message frame.
bool AFS_TuiHandleScrollEvent(ftxui::Event event, int& scroll_position, bool& follow_latest);
