#pragma once

#include "util/config_parser.hpp"

#include <nlohmann/json.hpp>
#include <string>

namespace flash::config::detail {

bool LoadJsonObjectFromFile(const std::string& path, nlohmann::json& out, std::string& err);
bool FillConfigFromJson(const nlohmann::json& j, SkytrackConfigFromFile& cfg, std::string& err);

} // namespace flash::config::detail
