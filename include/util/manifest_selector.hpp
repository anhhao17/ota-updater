#pragma once

#include "util/device_config.hpp"
#include "util/manifest.hpp"
#include "util/result.hpp"

namespace flash {

class ManifestSelector {
  public:
    static Result SelectForDevice(const Manifest& input, const DeviceConfig& device, Manifest& out);
};

} // namespace flash
