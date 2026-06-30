#pragma once

// ---- AFS_TuiKeyAction --------------------------------------------------------
// Semantic action produced by a keyboard binding. App state decides how the
// action is executed; this table only maps terminal keys to stable UI intents.
enum class AFS_TuiKeyAction {
    BeginExit,
    Submit,
    ToggleShellMode,
    OpenConfigMode,
    SaveConfig,
    InsertNewline,
    Scroll,
    CancelExitConfirmation,
};

// ---- AFS_TuiKeySignal --------------------------------------------------------
// Compact representation of FTXUI special keys and raw escape sequences.
enum class AFS_TuiKeySignal {
    Escape,
    Return,
    Tab,
    CtrlP,
    CtrlS,
    Backspace,
    Delete,
    ArrowLeft,
    ArrowRight,
    ArrowUp,
    ArrowDown,
    PageUp,
    PageDown,
    Home,
    End,
    Sequence,
};

// ---- AFS_TuiKeyBinding -------------------------------------------------------
struct AFS_TuiKeyBinding {
    AFS_TuiKeySignal signal;
    const char* sequence;
    AFS_TuiKeyAction action;
    int scroll_delta;
    bool cancels_exit_confirmation;
    const char* display;
    const char* description;
};

// ---- AFS_TuiKeyBindings ------------------------------------------------------
inline constexpr AFS_TuiKeyBinding AFS_TuiKeyBindings[] = {
    {AFS_TuiKeySignal::Escape, nullptr, AFS_TuiKeyAction::BeginExit, 0, false, "Esc",
     "begin or confirm exit"},
    {AFS_TuiKeySignal::Return, nullptr, AFS_TuiKeyAction::Submit, 0, true, "Enter",
     "submit input or run shell command"},
    {AFS_TuiKeySignal::Tab, nullptr, AFS_TuiKeyAction::ToggleShellMode, 0, true, "Tab",
     "toggle Agent/Shell mode"},
    {AFS_TuiKeySignal::CtrlP, nullptr, AFS_TuiKeyAction::OpenConfigMode, 0, true, "Ctrl+P",
     "open config mode"},
    {AFS_TuiKeySignal::CtrlS, nullptr, AFS_TuiKeyAction::SaveConfig, 0, true, "Ctrl+S",
     "save config mode"},
    {AFS_TuiKeySignal::Sequence, "\x1B[13;2u", AFS_TuiKeyAction::InsertNewline, 0, true,
     "Shift+Enter", "insert newline"},
    {AFS_TuiKeySignal::Sequence, "\x1B[13;5u", AFS_TuiKeyAction::InsertNewline, 0, true,
     "Ctrl+Enter", "insert newline"},
    {AFS_TuiKeySignal::Sequence, "\x1B[13;6u", AFS_TuiKeyAction::InsertNewline, 0, true,
     "Ctrl+Shift+Enter", "insert newline"},
    {AFS_TuiKeySignal::Sequence, "\x1B[27;2;13~", AFS_TuiKeyAction::InsertNewline, 0, true,
     "legacy Shift+Enter", "insert newline"},
    {AFS_TuiKeySignal::Sequence, "\x1B[27;5;13~", AFS_TuiKeyAction::InsertNewline, 0, true,
     "legacy Ctrl+Enter", "insert newline"},
    {AFS_TuiKeySignal::Sequence, "\x1B[27;6;13~", AFS_TuiKeyAction::InsertNewline, 0, true,
     "legacy Ctrl+Shift+Enter", "insert newline"},
    {AFS_TuiKeySignal::ArrowUp, nullptr, AFS_TuiKeyAction::Scroll, -30, false, "ArrowUp",
     "scroll messages up"},
    {AFS_TuiKeySignal::ArrowDown, nullptr, AFS_TuiKeyAction::Scroll, 30, false, "ArrowDown",
     "scroll messages down"},
    {AFS_TuiKeySignal::PageUp, nullptr, AFS_TuiKeyAction::Scroll, -180, false, "PageUp",
     "page messages up"},
    {AFS_TuiKeySignal::PageDown, nullptr, AFS_TuiKeyAction::Scroll, 180, false, "PageDown",
     "page messages down"},
    {AFS_TuiKeySignal::Backspace, nullptr, AFS_TuiKeyAction::CancelExitConfirmation, 0, true,
     "Backspace", "cancel exit confirmation"},
    {AFS_TuiKeySignal::Delete, nullptr, AFS_TuiKeyAction::CancelExitConfirmation, 0, true, "Delete",
     "cancel exit confirmation"},
    {AFS_TuiKeySignal::ArrowLeft, nullptr, AFS_TuiKeyAction::CancelExitConfirmation, 0, true,
     "ArrowLeft", "cancel exit confirmation"},
    {AFS_TuiKeySignal::ArrowRight, nullptr, AFS_TuiKeyAction::CancelExitConfirmation, 0, true,
     "ArrowRight", "cancel exit confirmation"},
    {AFS_TuiKeySignal::Home, nullptr, AFS_TuiKeyAction::CancelExitConfirmation, 0, true, "Home",
     "cancel exit confirmation"},
    {AFS_TuiKeySignal::End, nullptr, AFS_TuiKeyAction::CancelExitConfirmation, 0, true, "End",
     "cancel exit confirmation"},
};
