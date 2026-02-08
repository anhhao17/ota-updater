#include "flash/manifest_selector.hpp"

namespace flash {

Result ManifestSelector::SelectForDevice(const Manifest& input,
                                         const DeviceConfig& device,
                                         Manifest& out) {
    if (device.current_slot.empty()) {
        return Result::Fail(-1, "device current_slot is empty");
    }
    if (device.hw_compatibility.empty()) {
        return Result::Fail(-1, "device hw_compatibility is empty");
    }

    if (input.slot_components.empty()) {
        return Result::Fail(-1, "manifest missing slot sections (slot-*)");
    }

    auto it = input.slot_components.find(device.current_slot);
    if (it == input.slot_components.end()) {
        return Result::Fail(-1, "manifest missing slot section: " + device.current_slot);
    }
    out = input;
    out.components = it->second;
    return Result::Ok();
}

} // namespace flash
