#pragma once

#include "flash/io.hpp"
#include "flash/result.hpp"

#include <cstdint>
#include <memory>
#include <span>
#include <string>

namespace flash {

std::string Sha256Hex(std::span<const std::uint8_t> data);
std::string Sha256Hex(IReader& reader);
Result Sha256HexFile(const std::string& path, std::string& out_hex);

class Sha256Hasher {
public:
    Sha256Hasher();
    Sha256Hasher(const Sha256Hasher&) = delete;
    Sha256Hasher& operator=(const Sha256Hasher&) = delete;
    Sha256Hasher(Sha256Hasher&&) noexcept;
    Sha256Hasher& operator=(Sha256Hasher&&) noexcept;
    ~Sha256Hasher();

    void Update(std::span<const std::uint8_t> data);
    std::string FinalHex();

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace flash
