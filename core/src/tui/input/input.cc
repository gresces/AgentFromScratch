#include "tui/input/input.hh"

#include <ftxui/screen/terminal.hpp>

#include <algorithm>
#include <cctype>
#include <string_view>

using namespace ftxui;

namespace {

constexpr int kScrollTop = 0;
constexpr int kScrollBottom = 1000;

// 计算单次滚动的最大增量：80% 页面高度
int maxScrollDelta() {
    int page_units = 1000 / 3;               // ≈333，保守估算
    return std::max(1, page_units * 8 / 10); // ≈266
}

// ---- signalMatches -----------------------------------------------------------
bool signalMatches(AFS_TuiKeySignal signal, const Event& event) {
    switch (signal) {
    case AFS_TuiKeySignal::Escape:
        return event == Event::Escape;
    case AFS_TuiKeySignal::Return:
        return event == Event::Return;
    case AFS_TuiKeySignal::Tab:
        return event == Event::Tab;
    case AFS_TuiKeySignal::CtrlP:
        return event == Event::CtrlP;
    case AFS_TuiKeySignal::CtrlS:
        return event == Event::CtrlS;
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

// ---- readline helpers --------------------------------------------------------
int clampedCursor(const std::string& input, int cursor_position) {
    return std::clamp(cursor_position, 0, static_cast<int>(input.size()));
}

bool isWordByte(char ch) {
    unsigned char value = static_cast<unsigned char>(ch);
    return std::isalnum(value) || ch == '_';
}

int currentLineBegin(const std::string& input, int cursor_position) {
    cursor_position = clampedCursor(input, cursor_position);
    auto line_break = input.rfind('\n', static_cast<std::size_t>(cursor_position));
    if (line_break == std::string::npos) return 0;
    if (static_cast<int>(line_break) == cursor_position && cursor_position > 0) {
        line_break = input.rfind('\n', static_cast<std::size_t>(cursor_position - 1));
        if (line_break == std::string::npos) return 0;
    }
    return static_cast<int>(line_break) + 1;
}

int currentLineEnd(const std::string& input, int cursor_position) {
    cursor_position = clampedCursor(input, cursor_position);
    auto line_break = input.find('\n', static_cast<std::size_t>(cursor_position));
    if (line_break == std::string::npos) return static_cast<int>(input.size());
    return static_cast<int>(line_break);
}

int previousWordBegin(const std::string& input, int cursor_position) {
    int position = clampedCursor(input, cursor_position);
    while (position > 0 && !isWordByte(input[static_cast<std::size_t>(position - 1)])) {
        --position;
    }
    while (position > 0 && isWordByte(input[static_cast<std::size_t>(position - 1)])) {
        --position;
    }
    return position;
}

int nextCharacterPosition(const std::string& input, int cursor_position) {
    cursor_position = clampedCursor(input, cursor_position);
    if (cursor_position >= static_cast<int>(input.size())) return cursor_position;
    return cursor_position + 1;
}

// ---- adjustScrollPosition ----------------------------------------------------
void adjustScrollPosition(int delta, int& scroll_position, bool& follow_latest) {
    // 限制滚动增量不超过 80% 页面高度
    int max_delta = maxScrollDelta();
    if (delta > 0)
        delta = std::min(delta, max_delta);
    else if (delta < 0)
        delta = std::max(delta, -max_delta);

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

bool AFS_TuiHandleReadlineShortcut(const Event& event, std::string& input, int& cursor_position) {
    cursor_position = clampedCursor(input, cursor_position);

    if (event == Event::CtrlA) {
        cursor_position = currentLineBegin(input, cursor_position);
        return true;
    }
    if (event == Event::CtrlE) {
        cursor_position = currentLineEnd(input, cursor_position);
        return true;
    }
    if (event == Event::CtrlB) {
        if (cursor_position > 0) --cursor_position;
        return true;
    }
    if (event == Event::CtrlF) {
        cursor_position = nextCharacterPosition(input, cursor_position);
        return true;
    }
    if (event == Event::CtrlW) {
        int start = previousWordBegin(input, cursor_position);
        if (start == cursor_position) return true;
        input.erase(static_cast<std::size_t>(start),
                    static_cast<std::size_t>(cursor_position - start));
        cursor_position = start;
        return true;
    }
    if (event == Event::CtrlU) {
        int start = currentLineBegin(input, cursor_position);
        input.erase(static_cast<std::size_t>(start),
                    static_cast<std::size_t>(cursor_position - start));
        cursor_position = start;
        return true;
    }
    if (event == Event::CtrlK) {
        int end = currentLineEnd(input, cursor_position);
        input.erase(static_cast<std::size_t>(cursor_position),
                    static_cast<std::size_t>(end - cursor_position));
        return true;
    }
    if (event == Event::CtrlD) {
        if (cursor_position >= static_cast<int>(input.size())) return true;
        input.erase(static_cast<std::size_t>(cursor_position), 1);
        return true;
    }

    return false;
}

bool AFS_TuiHandleScrollEvent(Event event, int& scroll_position, bool& follow_latest) {
    if (event.is_mouse()) {
        auto& mouse = event.mouse();
        int max_delta = maxScrollDelta();
        // 鼠标滚轮步长取 80% 页面高度的 1/6，约等于当前一行的高度
        int wheel_step = std::max(1, max_delta / 6);
        if (mouse.button == Mouse::WheelUp) {
            adjustScrollPosition(-wheel_step, scroll_position, follow_latest);
            return true;
        }
        if (mouse.button == Mouse::WheelDown) {
            adjustScrollPosition(wheel_step, scroll_position, follow_latest);
            return true;
        }
        return false;
    }

    auto action = AFS_TuiResolveKeyAction(event);
    if (!action.has_value() || action->action != AFS_TuiKeyAction::Scroll) return false;

    adjustScrollPosition(action->scroll_delta, scroll_position, follow_latest);
    return true;
}