#include "crypto/sha256.hpp"
#include "ota/staging_verifier.hpp"
#include "testing.hpp"

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <gtest/gtest.h>
#include <memory>
#include <string>
#include <vector>

namespace flash {
namespace {

std::string ReadAll(IReader& reader) {
    std::string out;
    std::vector<std::uint8_t> buf(1024);
    while (true) {
        const ssize_t n = reader.Read(std::span<std::uint8_t>(buf.data(), buf.size()));
        if (n == 0)
            break;
        if (n < 0)
            return {};
        out.append(reinterpret_cast<const char*>(buf.data()), static_cast<size_t>(n));
    }
    return out;
}

TEST(StagingVerifierTest, StageAndVerifyAcceptsCorrectHash) {
    const std::string payload = "kernel-image-content";
    const std::string expected = Sha256Hex(std::span<const std::uint8_t>(
        reinterpret_cast<const std::uint8_t*>(payload.data()), payload.size()));

    OtaEntryStager stager;
    StagedEntry staged;
    std::unique_ptr<IReader> source = std::make_unique<testutil::MemoryReader>(payload);

    const auto res = stager.StageAndVerify(source, expected, staged);
    ASSERT_TRUE(res.is_ok()) << res.msg;
    ASSERT_NE(staged.reader, nullptr);
    EXPECT_EQ(ReadAll(*staged.reader), payload);
}

TEST(StagingVerifierTest, StageAndVerifyRejectsTamperedContent) {
    const std::string payload = "bootloader-v2";
    const std::string wrong_hash(64, '0');

    OtaEntryStager stager;
    StagedEntry staged;
    std::unique_ptr<IReader> source = std::make_unique<testutil::MemoryReader>(payload);

    const auto res = stager.StageAndVerify(source, wrong_hash, staged);
    ASSERT_FALSE(res.is_ok());
    EXPECT_NE(res.msg.find("sha256 mismatch"), std::string::npos);
}

TEST(StagingVerifierTest, StageAndVerifyAcceptsUppercaseExpectedHash) {
    const std::string payload = "filesystem-archive";
    std::string expected = Sha256Hex(std::span<const std::uint8_t>(
        reinterpret_cast<const std::uint8_t*>(payload.data()), payload.size()));
    std::transform(expected.begin(), expected.end(), expected.begin(), [](unsigned char c) {
        return static_cast<char>(std::toupper(c));
    });

    OtaEntryStager stager;
    StagedEntry staged;
    std::unique_ptr<IReader> source = std::make_unique<testutil::MemoryReader>(payload);

    const auto res = stager.StageAndVerify(source, expected, staged);
    ASSERT_TRUE(res.is_ok()) << res.msg;
    ASSERT_NE(staged.reader, nullptr);
    EXPECT_EQ(ReadAll(*staged.reader), payload);
}

TEST(StagingVerifierTest, StageAndVerifyRejectsEmptyExpectedHash) {
    OtaEntryStager stager;
    StagedEntry staged;
    std::unique_ptr<IReader> source = std::make_unique<testutil::MemoryReader>("abc");

    const auto res = stager.StageAndVerify(source, "", staged);
    ASSERT_FALSE(res.is_ok());
    EXPECT_NE(res.msg.find("expected sha256 is empty"), std::string::npos);
}

} // namespace
} // namespace flash
