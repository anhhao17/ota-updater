#pragma once

#include "io/io.hpp"

#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <optional>
#include <span>
#include <stdexcept>
#include <string>
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

} // namespace testutil
