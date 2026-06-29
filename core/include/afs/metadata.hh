#pragma once

#include <unordered_map>
#include <string>

using MetaData = std::unordered_map<std::string, std::string>;

// ---- helpers ----------------------------------------------------------------
inline void appendMeta(std::string& out, const std::unordered_map<std::string, std::string>& meta) {
    if (meta.empty()) return;
    out += " {";
    bool first = true;
    for (const auto& [key, value] : meta) {
        if (!first) out += ", ";
        out += key + "=" + value;
        first = false;
    }
    out += "}";
}
