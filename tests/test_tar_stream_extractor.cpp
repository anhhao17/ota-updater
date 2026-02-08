#include "ota/tar_stream_extractor.hpp"
#include "testing.hpp"
#include "util/logger.hpp"

#include <archive.h>
#include <archive_entry.h>
#include <cstdint>
#include <filesystem>
#include <fstream>
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

std::string ReadFile(const std::filesystem::path& p) {
    std::ifstream ifs(p);
    return std::string(std::istreambuf_iterator<char>(ifs), std::istreambuf_iterator<char>());
}

class RecordingProgress final : public IProgress {
  public:
    struct Snapshot {
        std::string component;
        std::uint64_t comp_done = 0;
        std::uint64_t comp_total = 0;
        std::uint64_t overall_done = 0;
        std::uint64_t overall_total = 0;
    };

    void OnProgress(const ProgressEvent& e) override {
        calls.push_back(Snapshot{
            .component = std::string(e.component),
            .comp_done = e.comp_done,
            .comp_total = e.comp_total,
            .overall_done = e.overall_done,
            .overall_total = e.overall_total,
        });
    }

    std::vector<Snapshot> calls;
};

class TarStreamExtractorTest : public ::testing::Test {
  protected:
    void SetUp() override {
        prev_level_ = flash::Logger::Instance().Level();
        flash::Logger::Instance().SetLevel(flash::LogLevel::Debug);
    }

    void TearDown() override { flash::Logger::Instance().SetLevel(prev_level_); }

  private:
    flash::LogLevel prev_level_{flash::LogLevel::Info};
};

TEST_F(TarStreamExtractorTest, ExtractsIntoDestinationWithoutChangingCwd) {
    namespace fs = std::filesystem;
    testutil::TemporaryDirectory temp;
    const fs::path dst = fs::path(temp.Path()) / "extract";
    fs::create_directories(dst);

    auto tar = BuildTar({
        {"sub/one.txt", "abc", AE_IFREG},
        {"two.txt", "xyz", AE_IFREG},
    });

    testutil::MemoryReader reader(std::move(tar));
    TarStreamExtractor extractor;

    const fs::path cwd_before = fs::current_path();
    auto res = extractor.ExtractToDir(reader, dst.string(), "archive");
    const fs::path cwd_after = fs::current_path();

    ASSERT_TRUE(res.is_ok()) << res.msg;
    EXPECT_EQ(cwd_after, cwd_before);
    EXPECT_EQ(ReadFile(dst / "sub" / "one.txt"), "abc");
    EXPECT_EQ(ReadFile(dst / "two.txt"), "xyz");
}

TEST_F(TarStreamExtractorTest, RejectsUnsafePathWhenSafeModeEnabled) {
    namespace fs = std::filesystem;
    testutil::TemporaryDirectory temp;
    const fs::path dst = fs::path(temp.Path()) / "extract";
    fs::create_directories(dst);

    auto tar = BuildTar({
        {"../escape.txt", "boom", AE_IFREG},
    });

    testutil::MemoryReader reader(std::move(tar));
    TarStreamExtractor::Options opt;
    opt.safe_paths_only = true;
    TarStreamExtractor extractor(opt);

    auto res = extractor.ExtractToDir(reader, dst.string(), "archive");
    ASSERT_FALSE(res.is_ok());
    EXPECT_NE(res.msg.find("Unsafe path in archive"), std::string::npos);
    EXPECT_FALSE(fs::exists(fs::path(temp.Path()) / "escape.txt"));
}

TEST_F(TarStreamExtractorTest, ReportsProgressToSink) {
    namespace fs = std::filesystem;
    testutil::TemporaryDirectory temp;
    const fs::path dst = fs::path(temp.Path()) / "extract";
    fs::create_directories(dst);

    auto tar = BuildTar({
        {"file.txt", "hello", AE_IFREG},
    });

    testutil::MemoryReader reader(std::move(tar));
    RecordingProgress progress;

    TarStreamExtractor::Options opt;
    opt.progress = false;
    opt.progress_sink = &progress;
    opt.component_total_bytes = 10;
    opt.overall_done_base_bytes = 100;
    opt.overall_total_bytes = 200;
    TarStreamExtractor extractor(opt);

    auto res = extractor.ExtractToDir(reader, dst.string(), "archive");
    ASSERT_TRUE(res.is_ok()) << res.msg;
    ASSERT_FALSE(progress.calls.empty());

    const auto& last = progress.calls.back();
    EXPECT_EQ(last.component, "archive");
    EXPECT_EQ(last.comp_done, 5U);
    EXPECT_EQ(last.comp_total, 10U);
    EXPECT_EQ(last.overall_done, 105U);
    EXPECT_EQ(last.overall_total, 200U);
}

} // namespace
} // namespace flash
