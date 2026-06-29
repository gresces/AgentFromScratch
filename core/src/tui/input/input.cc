#include "tui/input/input.hh"

#include <algorithm>
#include <initializer_list>
#include <string_view>

using namespace ftxui;

namespace {

bool matchesAny(std::string_view input, std::initializer_list<std::string_view> candidates) {
    for (std::string_view candidate : candidates) {
        if (input == candidate) return true;
    }
    return false;
}

} // namespace

bool AFS_TuiIsMultilineShortcut(const Event& event) {
    // Plain Enter is Event::Return / Event::CtrlJ (both are LF in FTXUI). Do not
    // match LF here: it is reserved for submit, and Ctrl+J must not be treated as
    // the multiline shortcut.
    const std::string& input = event.input();

    // CSI-u / kitty keyboard protocol / modern xterm modifyOtherKeys:
    //   ESC [ 13 ; 2 u  -> Shift+Enter
    //   ESC [ 13 ; 5 u  -> Ctrl+Enter
    //   ESC [ 13 ; 6 u  -> Ctrl+Shift+Enter
    // Some terminals use the legacy modifyOtherKeys form:
    //   ESC [ 27 ; <modifier> ; 13 ~
    return matchesAny(input, {
        "\x1B[13;2u",
        "\x1B[13;5u",
        "\x1B[13;6u",
        "\x1B[27;2;13~",
        "\x1B[27;5;13~",
        "\x1B[27;6;13~",
    });
}

bool AFS_TuiCancelsExitConfirmation(const Event& event) {
    return event.is_character() || event == Event::Return || event == Event::Backspace
           || event == Event::Delete || event == Event::ArrowLeft || event == Event::ArrowRight
           || event == Event::Home || event == Event::End || event == Event::Tab
           || AFS_TuiIsMultilineShortcut(event);
}

bool AFS_TuiHandleScrollEvent(Event event, int total_messages, int& scroll_offset) {
    if (event.is_mouse()) {
        auto& mouse = event.mouse();
        if (mouse.button == Mouse::WheelUp) {
            if (scroll_offset > 0) --scroll_offset;
            return true;
        }
        if (mouse.button == Mouse::WheelDown) {
            if (total_messages > 0 && scroll_offset < total_messages - 1) ++scroll_offset;
            return true;
        }
        return false;
    }

    if (event != Event::ArrowUp && event != Event::ArrowDown && event != Event::PageUp
        && event != Event::PageDown) {
        return false;
    }

    if (total_messages == 0) return false;

    if (event == Event::ArrowUp) {
        if (scroll_offset > 0) --scroll_offset;
    } else if (event == Event::ArrowDown) {
        if (scroll_offset < total_messages - 1) ++scroll_offset;
    } else if (event == Event::PageUp) {
        scroll_offset = std::max(0, scroll_offset - 10);
    } else if (event == Event::PageDown) {
        scroll_offset = std::min(total_messages - 1, scroll_offset + 10);
    }
    return true;
}
