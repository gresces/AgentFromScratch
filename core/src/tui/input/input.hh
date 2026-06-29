#pragma once

#include <ftxui/component/event.hpp>

// Return true for modified Enter key sequences that terminals send when
// Ctrl+Enter or Shift+Enter are distinguishable from plain Enter.
bool AFS_TuiIsMultilineShortcut(const ftxui::Event& event);

// Return true when an event should cancel the "press Esc again to exit" state.
bool AFS_TuiCancelsExitConfirmation(const ftxui::Event& event);

// Apply mouse/keyboard scrolling. Returns true when the event was consumed.
bool AFS_TuiHandleScrollEvent(ftxui::Event event, int total_messages, int& scroll_offset);
