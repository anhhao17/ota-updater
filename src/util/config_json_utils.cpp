#include "util/config_json_utils.hpp"

#include <fstream>

namespace flash::config::detail {

namespace {

bool GetStringIfPresent(const nlohmann::json& j, const char* key, std::string& out) {
    auto it = j.find(key);
    if (it == j.end())
        return false;
    if (!it->is_string())
        return false;
    out = it->get<std::string>();
    return true;
}

bool GetU64IfPresent(const nlohmann::json& j, const char* key, std::uint64_t& out) {
    auto it = j.find(key);
    if (it == j.end())
        return false;
    if (!(it->is_number_unsigned() || it->is_number_integer()))
        return false;
    auto v = it->get<long long>();
    if (v < 0)
        return false;
    out = static_cast<std::uint64_t>(v);
    return true;
}

bool GetBoolIfPresent(const nlohmann::json& j, const char* key, bool& out) {
    auto it = j.find(key);
    if (it == j.end())
        return false;
    if (!it->is_boolean())
        return false;
    out = it->get<bool>();
    return true;
}

} // namespace

bool LoadJsonObjectFromFile(const std::string& path, nlohmann::json& out, std::string& err) {
    std::ifstream is(path);
    if (!is.good()) {
        err = "cannot open " + path;
        return false;
    }

    try {
        is >> out;
    } catch (const std::exception& e) {
        err = "invalid JSON in " + path + ": " + e.what();
        return false;
    }

    if (!out.is_object()) {
        err = "root must be JSON object: " + path;
        return false;
    }

    return true;
}

bool FillConfigFromJson(const nlohmann::json& j, SkytrackConfigFromFile& cfg, std::string& err) {
    GetStringIfPresent(j, "RootfsPartA", cfg.rootfs_part_a);
    GetStringIfPresent(j, "RootfsPartB", cfg.rootfs_part_b);

    if (cfg.rootfs_part_a.empty() || cfg.rootfs_part_b.empty()) {
        err = "missing RootfsPartA/RootfsPartB";
        return false;
    }
    if (cfg.rootfs_part_a == cfg.rootfs_part_b) {
        err = "RootfsPartA and RootfsPartB must differ";
        return false;
    }

    {
        std::uint64_t v{};
        if (GetU64IfPresent(j, "FsyncIntervalBytes", v)) {
            cfg.fsync_interval_bytes = v;
        }
    }
    {
        bool b{};
        if (GetBoolIfPresent(j, "Progress", b)) {
            cfg.progress = b;
        }
    }

    return true;
}

} // namespace flash::config::detail
