#include "util/device_config.hpp"

#include <fstream>
#include <nlohmann/json.hpp>
#include <string>

namespace flash {

namespace {

bool GetString(const nlohmann::json& j, const char* key, std::string& out) {
    auto it = j.find(key);
    if (it == j.end())
        return false;
    if (!it->is_string())
        return false;
    out = it->get<std::string>();
    return true;
}

} // namespace

Result DeviceConfig::LoadFromFile(const std::string& path, DeviceConfig& out) {
    out = DeviceConfig{};

    std::ifstream is(path);
    if (!is.good()) {
        return Result::Fail(-1, "cannot open device config: " + path);
    }

    nlohmann::json j;
    try {
        is >> j;
    } catch (const std::exception& e) {
        return Result::Fail(-1, std::string("invalid JSON in ") + path + ": " + e.what());
    }

    if (!j.is_object()) {
        return Result::Fail(-1, "device config must be JSON object: " + path);
    }

    if (!GetString(j, "current_slot", out.current_slot)) {
        (void)GetString(j, "current-slot", out.current_slot);
    }
    if (!GetString(j, "hw_compatibility", out.hw_compatibility)) {
        (void)GetString(j, "hw-compatibility", out.hw_compatibility);
    }

    if (out.current_slot.empty()) {
        return Result::Fail(-1, "device config missing current_slot");
    }
    if (out.hw_compatibility.empty()) {
        return Result::Fail(-1, "device config missing hw_compatibility");
    }

    return Result::Ok();
}

} // namespace flash
