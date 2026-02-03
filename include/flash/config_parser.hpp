#pragma once
#include <cstdint>
#include <optional>
#include <string>

namespace flash::config {

class SkytrackConfigFromFile {
public:
    std::string rootfs_part_a;
    std::string rootfs_part_b;

    std::optional<std::uint64_t> fsync_interval_bytes;
    std::optional<bool> progress;

    bool LoadFile(const std::string &path);

    void Reset();
};

} // namespace flash::config
