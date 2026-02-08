#pragma once
#include "io/io.hpp"
#include <cstdint>
#include <memory>

namespace flash {

class CountingReader final : public IReader {
public:
    explicit CountingReader(std::unique_ptr<IReader> inner, std::uint64_t* external_counter = nullptr)
        : inner_(std::move(inner)), external_(external_counter) {}

    ssize_t Read(std::span<std::uint8_t> out) override {
        const ssize_t n = inner_->Read(out);
        if (n > 0) {
            read_ += static_cast<std::uint64_t>(n);
            if (external_) *external_ = read_;
        }
        return n;
    }

    std::uint64_t BytesRead() const { return read_; }

    std::optional<std::uint64_t> TotalSize() const override {
        return inner_ ? inner_->TotalSize() : std::nullopt;
    }

private:
    std::unique_ptr<IReader> inner_;
    std::uint64_t read_ = 0;
    std::uint64_t* external_ = nullptr;
};

} // namespace flash
