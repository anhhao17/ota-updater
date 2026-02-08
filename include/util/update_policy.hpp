#pragma once

#include "util/manifest.hpp"

#include <string>

namespace flash {

class UpdatePolicy {
  public:
    static bool ShouldUpdate(const Component& comp,
                             const Manifest& manifest,
                             const std::string& current_version);
};

} // namespace flash
