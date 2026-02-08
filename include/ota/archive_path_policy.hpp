#pragma once

#include "util/result.hpp"

#include <string>

namespace flash {

class ArchivePathPolicy {
  public:
    explicit ArchivePathPolicy(bool safe_paths_only) : safe_paths_only_(safe_paths_only) {}

    Result NormalizeEntryPath(const char* raw_path, std::string& out_relative) const;
    Result NormalizeHardlinkPath(const char* raw_path, std::string& out_relative) const;

  private:
    static bool IsSafeRelativePath(const std::string& p);

    bool safe_paths_only_ = true;
};

} // namespace flash
