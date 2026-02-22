#pragma once

#include "io/io.hpp"

#include <archive.h>
#include <archive_entry.h>
#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdlib>
#include <optional>
#include <span>
#include <stdexcept>
#include <string>
#include <sys/stat.h>
#include <unistd.h>
#include <utility>
#include <vector>

namespace testutil {

class TemporaryDirectory {
  public:
    TemporaryDirectory() {
        char tpl[] = "/tmp/flash_tool_tests_XXXXXX";
        char* p = ::mkdtemp(tpl);
        if (!p) {
            throw std::runtime_error("mkdtemp failed");
        }
        path_ = p;
    }

    ~TemporaryDirectory() {
        // Best-effort cleanup. Keep it simple: rely on "rm -rf".
        if (!path_.empty()) {
            std::string cmd = "rm -rf '" + path_ + "'";
            (void)::system(cmd.c_str());
        }
    }

    TemporaryDirectory(const TemporaryDirectory&) = delete;
    TemporaryDirectory& operator=(const TemporaryDirectory&) = delete;

    const std::string& Path() const { return path_; }

  private:
    std::string path_;
};

class MemoryReader final : public flash::IReader {
  public:
    explicit MemoryReader(std::string data) : data_(data.begin(), data.end()) {}

    explicit MemoryReader(std::vector<std::uint8_t> data) : data_(std::move(data)) {}

    ssize_t Read(std::span<std::uint8_t> out) override {
        if (pos_ >= data_.size())
            return 0;
        const size_t n = std::min(out.size(), data_.size() - pos_);
        std::copy(data_.begin() + static_cast<std::ptrdiff_t>(pos_),
                  data_.begin() + static_cast<std::ptrdiff_t>(pos_ + n),
                  out.begin());
        pos_ += n;
        return static_cast<ssize_t>(n);
    }

    std::optional<std::uint64_t> TotalSize() const override {
        return static_cast<std::uint64_t>(data_.size());
    }

  private:
    std::vector<std::uint8_t> data_;
    size_t pos_ = 0;
};

struct TarEntry {
    std::string path;
    std::string contents;
    mode_t file_type;
};

inline std::vector<std::uint8_t> BuildTar(const std::vector<TarEntry>& entries) {
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

inline std::string ReadAll(flash::IReader& reader) {
    std::string out;
    std::array<std::uint8_t, 1024> buf{};
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

} // namespace testutil
