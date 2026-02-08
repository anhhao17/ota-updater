#include "util/config_parser.hpp"

#include "util/config_json_utils.hpp"

#include <cstdio>

namespace flash::config {

void SkytrackConfigFromFile::Reset() {
    rootfs_part_a.clear();
    rootfs_part_b.clear();
    fsync_interval_bytes.reset();
    progress.reset();
}

bool SkytrackConfigFromFile::LoadFile(const std::string& path) {
    Reset();

    nlohmann::json json;
    std::string err;
    if (!detail::LoadJsonObjectFromFile(path, json, err)) {
        std::fprintf(stderr, "Config: %s\n", err.c_str());
        return false;
    }

    if (!detail::FillConfigFromJson(json, *this, err)) {
        std::fprintf(stderr, "Config: %s in %s\n", err.c_str(), path.c_str());
        return false;
    }

    return true;
}

} // namespace flash::config
