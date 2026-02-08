#include "ota/ota_bundle_reader.hpp"
#include "testing.hpp"

#include <archive.h>
#include <archive_entry.h>
#include <array>
#include <cstdint>
#include <gtest/gtest.h>
#include <memory>
#include <stdexcept>
#include <string>
#include <sys/stat.h>
#include <utility>
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

std::string ReadAll(IReader& reader) {
    std::string out;
    std::array<std::uint8_t, 128> buf{};
    while (true) {
        const ssize_t n = reader.Read(std::span<std::uint8_t>(buf.data(), buf.size()));
        if (n <= 0)
            break;
        out.append(reinterpret_cast<const char*>(buf.data()), static_cast<size_t>(n));
    }
    return out;
}

TEST(OtaTarBundleReaderTest, ReadsRegularEntriesAndSkipsDirectories) {
    auto tar = BuildTar({
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
    EXPECT_EQ(ReadAll(*entry_reader), "payload");

    ASSERT_TRUE(reader.Next(info, eof).is_ok());
    EXPECT_TRUE(eof);
}

TEST(OtaTarBundleReaderTest, EnforcesEntryCompletionBeforeNext) {
    auto tar = BuildTar({
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
