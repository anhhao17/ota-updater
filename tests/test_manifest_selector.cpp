#include "util/manifest_selector.hpp"

#include <gtest/gtest.h>
#include <string>

namespace flash {

TEST(ManifestSelectorTest, SelectsComponentsForMatchingSlotAndHardware) {
    Manifest manifest;
    manifest.hw_compatibility = "jetson-orin";
    manifest.slot_components["slot-a"] = {Component{.name = "rootfs", .filename = "rootfs.tar.gz"}};

    DeviceConfig device;
    device.current_slot = "slot-a";
    device.hw_compatibility = "jetson-orin";

    Manifest selected;
    auto res = ManifestSelector::SelectForDevice(manifest, device, selected);
    ASSERT_TRUE(res.is_ok()) << res.msg;
    ASSERT_EQ(selected.components.size(), 1U);
    EXPECT_EQ(selected.components[0].name, "rootfs");
}

TEST(ManifestSelectorTest, RejectsHardwareMismatch) {
    Manifest manifest;
    manifest.hw_compatibility = "board-a";
    manifest.slot_components["slot-a"] = {Component{.name = "rootfs", .filename = "rootfs.tar.gz"}};

    DeviceConfig device;
    device.current_slot = "slot-a";
    device.hw_compatibility = "board-b";

    Manifest selected;
    auto res = ManifestSelector::SelectForDevice(manifest, device, selected);
    ASSERT_FALSE(res.is_ok());
    EXPECT_NE(res.msg.find("hw_compatibility mismatch"), std::string::npos);
}

} // namespace flash
