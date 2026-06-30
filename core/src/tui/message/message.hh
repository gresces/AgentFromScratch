#pragma once

#include <string>

// ---- TuiMessage -------------------------------------------------------------
struct TuiMessage {
    enum Role { User, Assistant, Thinking, Tool, Shell };

    Role role;
    std::string content;
    std::string detail;
    bool append = false;
};
