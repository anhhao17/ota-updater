#include "crypto/sha256.hpp"
#include "ota/ota_bundle_reader.hpp"
#include "ota/ota_install_services.hpp"
#include "ota/update_module.hpp"
#include "testing.hpp"

#include <gtest/gtest.h>
#include <string>

namespace flash {
namespace {

std::string HashOf(const std::string& s) {
    return Sha256Hex(
        std::span<const std::uint8_t>(reinterpret_cast<const std::uint8_t*>(s.data()), s.size()));
}

TEST(OtaInstallServicesTest, FailsWhenManifestEntryIsMissingFromBundle) {
    testutil::TemporaryDirectory tmp;
    const std::string payload_a = "a-content";
    const std::string payload_b = "b-content";
    const std::string file_a = tmp.Path() + "/a.txt";
    const std::string file_b = tmp.Path() + "/b.txt";

    const std::string manifest_json =
        std::string("{\"version\":\"1.0.0\",\"hw_compatibility\":\"board-x\",\"components\":[") +
        "{\"name\":\"a\",\"type\":\"file\",\"filename\":\"a.bin\",\"path\":\"" + file_a +
        "\",\"sha256\":\"" + HashOf(payload_a) + "\"}," +
        "{\"name\":\"b\",\"type\":\"file\",\"filename\":\"b.bin\",\"path\":\"" + file_b +
        "\",\"sha256\":\"" + HashOf(payload_b) + "\"}" + "]}";

    auto tar = testutil::BuildTar({
        {"manifest.json", manifest_json, AE_IFREG},
        {"a.bin", payload_a, AE_IFREG},
    });

    testutil::MemoryReader source(std::move(tar));
    OtaTarBundleReader bundle;
    ASSERT_TRUE(bundle.Open(source).is_ok());

    Manifest manifest;
    auto load_res = ManifestLoader::LoadFromFirstBundleEntry(bundle, manifest);
    ASSERT_TRUE(load_res.is_ok()) << load_res.msg;

    const ComponentIndex index(manifest);
    UpdateModule module;
    InstallCoordinator coordinator(module, nullptr);

    auto install_res = coordinator.InstallMatchingEntries(bundle, index, 0);
    ASSERT_FALSE(install_res.is_ok());
    EXPECT_NE(install_res.msg.find("manifest component entry missing from ota.tar: b.bin"),
              std::string::npos);
}

} // namespace
} // namespace flash
