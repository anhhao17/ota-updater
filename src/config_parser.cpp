#include "flash/config_parser.hpp"

#include <cstdio>
#include <fstream>
#include <string>

#include <nlohmann/json.hpp>

namespace flash::config {

void SkytrackConfigFromFile::Reset() {
    rootfs_part_a.clear();
    rootfs_part_b.clear();
    fsync_interval_bytes.reset();
    progress.reset();
}

static bool GetStringIfPresent(const nlohmann::json &j, const char *key, std::string &out) {
    auto it = j.find(key);
    if (it == j.end()) return false;
    if (!it->is_string()) return false;
    out = it->get<std::string>();
    return true;
}

static bool GetU64IfPresent(const nlohmann::json &j, const char *key, std::uint64_t &out) {
    auto it = j.find(key);
    if (it == j.end()) return false;
    if (!(it->is_number_unsigned() || it->is_number_integer())) return false;
    auto v = it->get<long long>();
    if (v < 0) return false;
    out = static_cast<std::uint64_t>(v);
    return true;
}

static bool GetBoolIfPresent(const nlohmann::json &j, const char *key, bool &out) {
    auto it = j.find(key);
    if (it == j.end()) return false;
    if (!it->is_boolean()) return false;
    out = it->get<bool>();
    return true;
}

bool SkytrackConfigFromFile::LoadFile(const std::string &path) {
    Reset();

    std::ifstream is(path);
    if (!is.good()) {
        std::fprintf(stderr, "Config: cannot open %s\n", path.c_str());
        return false;
    }

    nlohmann::json j;
    try {
        is >> j;
    } catch (const std::exception &e) {
        std::fprintf(stderr, "Config: invalid JSON in %s: %s\n", path.c_str(), e.what());
        return false;
    }

    if (!j.is_object()) {
        std::fprintf(stderr, "Config: root must be JSON object: %s\n", path.c_str());
        return false;
    }

    // Required
    GetStringIfPresent(j, "RootfsPartA", rootfs_part_a);
    GetStringIfPresent(j, "RootfsPartB", rootfs_part_b);

    if (rootfs_part_a.empty() || rootfs_part_b.empty()) {
        std::fprintf(stderr, "Config: missing RootfsPartA/RootfsPartB in %s\n", path.c_str());
        return false;
    }
    if (rootfs_part_a == rootfs_part_b) {
        std::fprintf(stderr, "Config: RootfsPartA and RootfsPartB must differ\n");
        return false;
    }

    // Optional fields (future-proof)
    {
        std::uint64_t v{};
        if (GetU64IfPresent(j, "FsyncIntervalBytes", v)) {
            fsync_interval_bytes = v;
        }
    }
    {
        bool b{};
        if (GetBoolIfPresent(j, "Progress", b)) {
            progress = b;
        }
    }

    return true;
}

} // namespace flash::config
