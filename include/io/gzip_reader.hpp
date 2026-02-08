#pragma once

#include "io/io.hpp"

#include <memory>
#include <vector>
#include <zlib.h>

namespace flash {

class GzipReader final : public IReader {
  public:
    explicit GzipReader(std::unique_ptr<IReader> source);
    ~GzipReader() override;

    // Implementation of IReader
    ssize_t Read(std::span<std::uint8_t> out) override;
    std::optional<std::uint64_t> TotalSize() const override { return std::nullopt; }

  private:
    std::unique_ptr<IReader> source_;
    z_stream strm_{};
    std::vector<std::uint8_t> in_buffer_;
    bool eof_reached_ = false;
    bool drained_source_ = false;
};

} // namespace flash
