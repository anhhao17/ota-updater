#pragma once

#include "ota/update_module.hpp"

#include <memory>
#include <vector>

namespace flash {

std::vector<std::unique_ptr<UpdateModule::IInstallerStrategy>> CreateDefaultInstallerStrategies();

} // namespace flash
