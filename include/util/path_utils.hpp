#pragma once

#include <string>
#include <string_view>

namespace flash {

inline bool IsDevPath(std::string_view s) {
    return s.rfind("/dev/", 0) == 0;
}

// Normalize tar path to a clean relative form:
// - strip leading "./"
// - strip leading "/" (avoid absolute)
// - collapse duplicate slashes
inline std::string NormalizeTarPath(std::string s) {
    while (s.rfind("./", 0) == 0) s.erase(0, 2);
    while (!s.empty() && s.front() == '/') s.erase(0, 1);

    std::string out;
    out.reserve(s.size());
    bool prev_slash = false;
    for (char c : s) {
        const bool slash = (c == '/');
        if (slash && prev_slash) continue;
        out.push_back(c);
        prev_slash = slash;
    }
    return out;
}

} // namespace flash
