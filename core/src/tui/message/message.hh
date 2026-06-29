#pragma once

#include <string>

// ---- TuiMessage -------------------------------------------------------------
struct TuiMessage {
    enum Role { User, Assistant, Tool, Shell };

    Role role;
    std::string content;
    std::string detail;
};
