#include "tui/input/input.hh"

#include <algorithm>
#include <string_view>

using namespace ftxui;

namespace {

constexpr int kScrollTop = 0;
constexpr int kScrollBottom = 1000;
constexpr int kWheelScrollStep = 30;

// ---- signalMatches -----------------------------------------------------------
bool signalMatches(AFS_TuiKeySignal signal, const Event& event) {
    switch (signal) {
    case AFS_TuiKeySignal::Escape:
        return event == Event::Escape;
    case AFS_TuiKeySignal::Return:
        return event == Event::Return;
    case AFS_TuiKeySignal::Tab:
        return event == Event::Tab;
    case AFS_TuiKeySignal::Backspace:
        return event == Event::Backspace;
    case AFS_TuiKeySignal::Delete:
        return event == Event::Delete;
    case AFS_TuiKeySignal::ArrowLeft:
        return event == Event::ArrowLeft;
    case AFS_TuiKeySignal::ArrowRight:
        return event == Event::ArrowRight;
    case AFS_TuiKeySignal::ArrowUp:
        return event == Event::ArrowUp;
    case AFS_TuiKeySignal::ArrowDown:
        return event == Event::ArrowDown;
    case AFS_TuiKeySignal::PageUp:
        return event == Event::PageUp;
    case AFS_TuiKeySignal::PageDown:
        return event == Event::PageDown;
    case AFS_TuiKeySignal::Home:
        return event == Event::Home;
    case AFS_TuiKeySignal::End:
        return event == Event::End;
    case AFS_TuiKeySignal::Sequence:
        return false;
    }
    return false;
}

// ---- bindingMatches ----------------------------------------------------------
bool bindingMatches(const AFS_TuiKeyBinding& binding, const Event& event) {
    if (binding.signal == AFS_TuiKeySignal::Sequence) {
        return std::string_view(event.input()) == binding.sequence;
    }
    return signalMatches(binding.signal, event);
}

// ---- adjustScrollPosition ----------------------------------------------------
void adjustScrollPosition(int delta, int& scroll_position, bool& follow_latest) {
    scroll_position = std::clamp(scroll_position + delta, kScrollTop, kScrollBottom);
    follow_latest = scroll_position == kScrollBottom;
}

} // namespace

std::optional<AFS_TuiKeyActionEvent> AFS_TuiResolveKeyAction(const Event& event) {
    for (const auto& binding : AFS_TuiKeyBindings) {
        if (!bindingMatches(binding, event)) continue;
        return AFS_TuiKeyActionEvent{
            .action = binding.action,
            .scroll_delta = binding.scroll_delta,
            .cancels_exit_confirmation = binding.cancels_exit_confirmation,
        };
    }
    return std::nullopt;
}

bool AFS_TuiIsMultilineShortcut(const Event& event) {
    auto action = AFS_TuiResolveKeyAction(event);
    return action.has_value() && action->action == AFS_TuiKeyAction::InsertNewline;
}

bool AFS_TuiCancelsExitConfirmation(const Event& event) {
    auto action = AFS_TuiResolveKeyAction(event);
    return event.is_character() || (action.has_value() && action->cancels_exit_confirmation);
}

bool AFS_TuiHandleScrollEvent(Event event, int& scroll_position, bool& follow_latest) {
    if (event.is_mouse()) {
        auto& mouse = event.mouse();
        if (mouse.button == Mouse::WheelUp) {
            adjustScrollPosition(-kWheelScrollStep, scroll_position, follow_latest);
            return true;
        }
        if (mouse.button == Mouse::WheelDown) {
            adjustScrollPosition(kWheelScrollStep, scroll_position, follow_latest);
            return true;
        }
        return false;
    }

    auto action = AFS_TuiResolveKeyAction(event);
    if (!action.has_value() || action->action != AFS_TuiKeyAction::Scroll) return false;

    adjustScrollPosition(action->scroll_delta, scroll_position, follow_latest);
    return true;
}
