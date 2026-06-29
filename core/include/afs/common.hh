#pragma once

#include <random>
#include <string>

namespace AFS {

// ---- UUID -------------------------------------------------------------------

// 8 位十六进制 UUID（自增计数器）。
inline std::string uuid8() {
    static unsigned counter = 0;
    char buf[9];
    snprintf(buf, sizeof(buf), "%08x", counter++);
    return buf;
}

// 16 位随机十六进制 UUID。
inline std::string uuid16() {
    static std::random_device rd;
    static std::mt19937_64 gen(rd());
    static std::uniform_int_distribution<uint64_t> dist;

    char buf[17];
    snprintf(buf, sizeof(buf), "%016lx", dist(gen));
    return buf;
}

} // namespace AFS
