#pragma once

#include "util/manifest.hpp"

#include <expected>
#include <string>

namespace flash {

class ManifestParser {
  public:
    std::expected<Manifest, std::string> Parse(const std::string& json_input) const;
};

} // namespace flash
