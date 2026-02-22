#include "crypto/sha256.hpp"
#include "ota/ota_bundle_reader.hpp"
#include "ota/ota_install_services.hpp"
#include "ota/update_module.hpp"
#include "testing.hpp"

#include <archive.h>
#include <archive_entry.h>
#include <gtest/gtest.h>
#include <stdexcept>
#include <string>
#include <sys/stat.h>
#include <vector>

namespace flash {
namespace {

struct TarEntry {
    std::string path;
    std::string contents;
    mode_t file_type;
};

std::vector<std::uint8_t> BuildTar(const std::vector<TarEntry>& entries) {
    std::vector<std::uint8_t> out(1024 * 1024);
    size_t used = 0;

    archive* a = archive_write_new();
    if (!a)
        throw std::runtime_error("archive_write_new failed");
    if (archive_write_set_format_pax_restricted(a) != ARCHIVE_OK) {
        (void)archive_write_free(a);
        throw std::runtime_error("archive_write_set_format_pax_restricted failed");
    }
    if (archive_write_open_memory(a, out.data(), out.size(), &used) != ARCHIVE_OK) {
        (void)archive_write_free(a);
        throw std::runtime_error("archive_write_open_memory failed");
    }

    for (const auto& entry : entries) {
        archive_entry* hdr = archive_entry_new();
        if (!hdr) {
            (void)archive_write_free(a);
            throw std::runtime_error("archive_entry_new failed");
        }
        archive_entry_set_pathname(hdr, entry.path.c_str());
        archive_entry_set_filetype(hdr, entry.file_type);
        archive_entry_set_perm(hdr, 0644);
        archive_entry_set_size(hdr, static_cast<la_int64_t>(entry.contents.size()));
        if (archive_write_header(a, hdr) != ARCHIVE_OK) {
            archive_entry_free(hdr);
            (void)archive_write_free(a);
            throw std::runtime_error("archive_write_header failed");
        }
        if (!entry.contents.empty()) {
            if (archive_write_data(a, entry.contents.data(), entry.contents.size()) < 0) {
                archive_entry_free(hdr);
                (void)archive_write_free(a);
                throw std::runtime_error("archive_write_data failed");
            }
        }
        archive_entry_free(hdr);
    }

    if (archive_write_close(a) != ARCHIVE_OK) {
        (void)archive_write_free(a);
        throw std::runtime_error("archive_write_close failed");
    }
    if (archive_write_free(a) != ARCHIVE_OK) {
        throw std::runtime_error("archive_write_free failed");
    }
    out.resize(used);
    return out;
}

std::string HashOf(const std::string& s) {
    return Sha256Hex(std::span<const std::uint8_t>(
        reinterpret_cast<const std::uint8_t*>(s.data()), s.size()));
}

TEST(OtaInstallServicesTest, FailsWhenManifestEntryIsMissingFromBundle) {
    testutil::TemporaryDirectory tmp;
    const std::string payload_a = "a-content";
    const std::string payload_b = "b-content";
    const std::string file_a = tmp.Path() + "/a.txt";
    const std::string file_b = tmp.Path() + "/b.txt";

    const std::string manifest_json =
        std::string("{\"version\":\"1.0.0\",\"hw_compatibility\":\"board-x\",\"components\":[")
        + "{\"name\":\"a\",\"type\":\"file\",\"filename\":\"a.bin\",\"path\":\"" + file_a +
        "\",\"sha256\":\"" + HashOf(payload_a) + "\"},"
        + "{\"name\":\"b\",\"type\":\"file\",\"filename\":\"b.bin\",\"path\":\"" + file_b +
        "\",\"sha256\":\"" + HashOf(payload_b) + "\"}"
        + "]}";

    auto tar = BuildTar({
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
