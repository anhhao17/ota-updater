#pragma once

#include "flash/io.hpp"
#include "flash/result.hpp"

#include <cstdint>
#include <string>
#include <string_view>

namespace flash {

class TarStreamExtractor {
public:
    struct Options {
        bool progress = true;
        std::uint64_t progress_interval_bytes = 4 * 1024 * 1024ULL;
        bool safe_paths_only = true;
    };

    TarStreamExtractor() = default;
    explicit TarStreamExtractor(Options opt) : opt_(opt) {}

    Result ExtractToDir(IReader& tar_stream, const std::string& dst_dir, std::string_view tag) const;

private:
    Options opt_{};
};

} // namespace flash
