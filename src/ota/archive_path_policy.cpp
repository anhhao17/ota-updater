#include "ota/archive_path_policy.hpp"

#include "util/path_utils.hpp"

#include <string_view>

namespace flash {

bool ArchivePathPolicy::IsSafeRelativePath(const std::string& p) {
    if (p.empty()) return false;
    if (p.front() == '/') return false;
    if (p.find('\\') != std::string::npos) return false;

    std::string_view sv(p);
    while (!sv.empty()) {
        while (!sv.empty() && sv.front() == '/') sv.remove_prefix(1);
        const auto pos = sv.find('/');
        const auto seg = sv.substr(0, pos);
        if (seg == "..") return false;
        if (pos == std::string_view::npos) break;
        sv.remove_prefix(pos);
    }
    return true;
}

Result ArchivePathPolicy::NormalizeEntryPath(const char* raw_path, std::string& out_relative) const {
    out_relative = NormalizeTarPath(raw_path ? std::string(raw_path) : std::string());
    if (out_relative.empty() || out_relative == ".") return Result::Ok();

    if (safe_paths_only_ && !IsSafeRelativePath(out_relative)) {
        return Result::Fail(-1, "Unsafe path in archive: " + out_relative);
    }
    return Result::Ok();
}

Result ArchivePathPolicy::NormalizeHardlinkPath(const char* raw_path, std::string& out_relative) const {
    if (!raw_path || !*raw_path) {
        out_relative.clear();
        return Result::Ok();
    }

    out_relative = NormalizeTarPath(std::string(raw_path));
    if (out_relative.empty() || out_relative == ".") return Result::Ok();

    if (safe_paths_only_ && !IsSafeRelativePath(out_relative)) {
        return Result::Fail(-1, "Unsafe hardlink target in archive: " + out_relative);
    }
    return Result::Ok();
}

} // namespace flash
