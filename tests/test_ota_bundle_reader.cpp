#include "ota/ota_bundle_reader.hpp"
#include "testing.hpp"

#include <gtest/gtest.h>
#include <memory>
#include <string>

namespace flash {
namespace {

TEST(OtaTarBundleReaderTest, ReadsRegularEntriesAndSkipsDirectories) {
    auto tar = testutil::BuildTar({
        {"manifest.json", "{\"v\":1}", AE_IFREG},
        {"nested", "", AE_IFDIR},
        {"nested/file.txt", "payload", AE_IFREG},
    });

    testutil::MemoryReader source(std::move(tar));
    OtaTarBundleReader reader;
    ASSERT_TRUE(reader.Open(source).is_ok());

    BundleEntryInfo info{};
    bool eof = false;
    ASSERT_TRUE(reader.Next(info, eof).is_ok());
    ASSERT_FALSE(eof);
    EXPECT_EQ(info.name, "manifest.json");
    EXPECT_EQ(info.size, 7U);

    std::string manifest;
    ASSERT_TRUE(reader.ReadCurrentToString(manifest).is_ok());
    EXPECT_EQ(manifest, "{\"v\":1}");

    ASSERT_TRUE(reader.Next(info, eof).is_ok());
    ASSERT_FALSE(eof);
    EXPECT_EQ(info.name, "nested/file.txt");

    std::unique_ptr<IReader> entry_reader;
    ASSERT_TRUE(reader.OpenCurrentEntryReader(entry_reader).is_ok());
    ASSERT_NE(entry_reader, nullptr);
    EXPECT_EQ(testutil::ReadAll(*entry_reader), "payload");

    ASSERT_TRUE(reader.Next(info, eof).is_ok());
    EXPECT_TRUE(eof);
}

TEST(OtaTarBundleReaderTest, EnforcesEntryCompletionBeforeNext) {
    auto tar = testutil::BuildTar({
        {"one.txt", "aaa", AE_IFREG},
        {"two.txt", "bbb", AE_IFREG},
    });

    testutil::MemoryReader source(std::move(tar));
    OtaTarBundleReader reader;
    ASSERT_TRUE(reader.Open(source).is_ok());

    BundleEntryInfo info{};
    bool eof = false;
    ASSERT_TRUE(reader.Next(info, eof).is_ok());

    auto next_res = reader.Next(info, eof);
    ASSERT_FALSE(next_res.is_ok());
    EXPECT_NE(next_res.msg.find("Previous entry not finished"), std::string::npos);

    ASSERT_TRUE(reader.SkipCurrent().is_ok());
    ASSERT_TRUE(reader.Next(info, eof).is_ok());
    EXPECT_EQ(info.name, "two.txt");
}

TEST(OtaTarBundleReaderTest, HandlesMissingOpenAndMissingCurrentEntry) {
    OtaTarBundleReader reader;
    BundleEntryInfo info{};
    bool eof = false;
    auto next_res = reader.Next(info, eof);
    ASSERT_FALSE(next_res.is_ok());
    EXPECT_NE(next_res.msg.find("Bundle not opened"), std::string::npos);

    std::string out;
    auto read_res = reader.ReadCurrentToString(out);
    ASSERT_FALSE(read_res.is_ok());
    EXPECT_NE(read_res.msg.find("No current entry"), std::string::npos);
}

} // namespace
} // namespace flash
