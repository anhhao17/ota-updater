#pragma once

#include "util/result.hpp"

#include <string>

namespace flash {

struct DeviceConfig {
    std::string current_slot;
    std::string hw_compatibility;

    static Result LoadFromFile(const std::string& path, DeviceConfig& out);
};

} // namespace flash
