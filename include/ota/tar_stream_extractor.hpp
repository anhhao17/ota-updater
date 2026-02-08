#pragma once

#include "io/io.hpp"
#include "ota/progress.hpp"
#include "util/result.hpp"

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

        IProgress* progress_sink = nullptr;
        std::uint64_t component_total_bytes = 0;
        std::uint64_t overall_total_bytes = 0;
        std::uint64_t overall_done_base_bytes = 0;
    };

    TarStreamExtractor() = default;
    explicit TarStreamExtractor(const Options& opt) : opt_(opt) {}

    Result
    ExtractToDir(IReader& tar_stream, const std::string& dst_dir, std::string_view tag) const;

  private:
    Options opt_{};
};

} // namespace flash
