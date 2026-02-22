#include "crypto/sha256.hpp"
#include "testing.hpp"

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <gtest/gtest.h>
#include <string>
#include <sys/wait.h>

namespace flash {
namespace {

std::string ShellQuote(const std::string& s) {
    std::string out = "'";
    for (const char c : s) {
        if (c == '\'') {
            out += "'\"'\"'";
        } else {
            out.push_back(c);
        }
    }
    out.push_back('\'');
    return out;
}

int ExitCodeFromSystem(int rc) {
    if (rc == -1) {
        return -1;
    }
    if (WIFEXITED(rc)) {
        return WEXITSTATUS(rc);
    }
    return -1;
}

bool WriteTextFile(const std::string& path, const std::string& content) {
    std::ofstream os(path);
    if (!os.good()) {
        return false;
    }
    os << content;
    return os.good();
}

bool WriteBytesFile(const std::string& path, const std::vector<std::uint8_t>& content) {
    std::ofstream os(path, std::ios::binary);
    if (!os.good()) {
        return false;
    }
    os.write(reinterpret_cast<const char*>(content.data()),
             static_cast<std::streamsize>(content.size()));
    return os.good();
}

std::string Sha256OfBytes(const std::vector<std::uint8_t>& content) {
    return Sha256Hex(std::span<const std::uint8_t>(content.data(), content.size()));
}

TEST(MainCliTest, RunsFullChainWithSlotSelectionAndArchiveInstall) {
    namespace fs = std::filesystem;
    testutil::TemporaryDirectory tmp;

    const fs::path base(tmp.Path());
    const fs::path config_path = base / "ota.conf";
    const fs::path ota_path = base / "bundle.tar";
    const fs::path file_output = base / "out" / "config.txt";
    const fs::path archive_output = base / "rootfs-out";

    const std::string file_payload = "version=42\n";
    const std::vector<std::uint8_t> rootfs_tar = testutil::BuildTar({
        {"etc/os-release", "NAME=TestOS\n", AE_IFREG},
        {"bin/app", "echo ok\n", AE_IFREG},
    });

    const std::string file_sha = Sha256Hex(std::span<const std::uint8_t>(
        reinterpret_cast<const std::uint8_t*>(file_payload.data()), file_payload.size()));
    const std::string rootfs_sha = Sha256OfBytes(rootfs_tar);

    const std::string rootfs_payload(
        reinterpret_cast<const char*>(rootfs_tar.data()), rootfs_tar.size());

    // slot-a deliberately references a missing filename so this test proves slot-b selection.
    const std::string manifest_json =
        std::string("{")
        + "\"version\":\"2.0.0\","
          "\"hw_compatibility\":\"board-z\","
          "\"slot-a\":{\"components\":[{"
          "\"name\":\"wrong-slot\","
          "\"type\":\"file\","
          "\"filename\":\"missing-from-bundle.txt\","
          "\"path\":\""
        + (base / "should-not-be-written.txt").string() +
        "\","
        "\"sha256\":\"" +
        file_sha +
        "\"}]},"
        "\"slot-b\":{\"components\":["
        "{"
        "\"name\":\"cfg\","
        "\"type\":\"file\","
        "\"filename\":\"cfg.txt\","
        "\"path\":\"" +
        file_output.string() +
        "\","
        "\"create-destination\":true,"
        "\"sha256\":\"" +
        file_sha +
        "\"},"
        "{"
        "\"name\":\"rootfs\","
        "\"type\":\"archive\","
        "\"filename\":\"rootfs.tar\","
        "\"path\":\"" +
        archive_output.string() +
        "\","
        "\"sha256\":\"" +
        rootfs_sha + "\"}"
        "]}}";

    const std::string config_json =
        R"({"current_slot":"slot-b","hw_compatibility":"board-z"})";

    ASSERT_TRUE(WriteTextFile(config_path.string(), config_json));

    const auto ota = testutil::BuildTar({
        {"manifest.json", manifest_json, AE_IFREG},
        {"cfg.txt", file_payload, AE_IFREG},
        {"rootfs.tar", rootfs_payload, AE_IFREG},
    });
    ASSERT_TRUE(WriteBytesFile(ota_path.string(), ota));

    const std::string cmd = "OTA_CONFIG_PATH=" + ShellQuote(config_path.string()) + " " +
                            ShellQuote(FLASH_TOOL_BIN) + " -i " +
                            ShellQuote(ota_path.string()) + " >/dev/null 2>&1";
    const int rc = std::system(cmd.c_str());
    ASSERT_EQ(ExitCodeFromSystem(rc), 0);

    std::ifstream cfg_is(file_output);
    ASSERT_TRUE(cfg_is.good());
    const std::string cfg_content((std::istreambuf_iterator<char>(cfg_is)),
                                  std::istreambuf_iterator<char>());
    EXPECT_EQ(cfg_content, file_payload);

    std::ifstream osr_is(archive_output / "etc" / "os-release");
    ASSERT_TRUE(osr_is.good());
    const std::string osr((std::istreambuf_iterator<char>(osr_is)), std::istreambuf_iterator<char>());
    EXPECT_EQ(osr, "NAME=TestOS\n");

    std::ifstream app_is(archive_output / "bin" / "app");
    ASSERT_TRUE(app_is.good());
    const std::string app((std::istreambuf_iterator<char>(app_is)), std::istreambuf_iterator<char>());
    EXPECT_EQ(app, "echo ok\n");
}

} // namespace
} // namespace flash
