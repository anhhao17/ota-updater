#pragma once

#include "flash/device_config.hpp"
#include "flash/manifest.hpp"
#include "flash/result.hpp"

namespace flash {

class ManifestSelector {
public:
    static Result SelectForDevice(const Manifest& input,
                                  const DeviceConfig& device,
                                  Manifest& out);
};

} // namespace flash
