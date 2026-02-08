#include <gtest/gtest.h>

#include "crypto/sha256.hpp"

#include <cstring>
#include <string>

namespace flash {

class StringReader final : public IReader {
public:
    explicit StringReader(std::string data) : data_(std::move(data)) {}

    ssize_t Read(std::span<std::uint8_t> out) override {
        if (pos_ >= data_.size()) return 0;
        const size_t n = std::min(out.size(), data_.size() - pos_);
        std::memcpy(out.data(), data_.data() + pos_, n);
        pos_ += n;
        return static_cast<ssize_t>(n);
    }

private:
    std::string data_;
    size_t pos_ = 0;
};

TEST(Sha256Test, KnownVector) {
    const std::string input = "abc";
    const std::string expected =
        "ba7816bf8f01cfea414140de5dae2223"
        "b00361a396177a9cb410ff61f20015ad";

    StringReader reader(input);
    const std::string actual = Sha256Hex(reader);
    EXPECT_EQ(actual, expected);
}

} // namespace flash
