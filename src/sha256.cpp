#include "flash/sha256.hpp"

#include "flash/file_reader.hpp"

#include <openssl/evp.h>

#include <array>
#include <cstdint>
#include <vector>

namespace flash {

namespace {

std::string HexEncode(std::span<const std::uint8_t> bytes) {
    static constexpr char kHex[] = "0123456789abcdef";
    std::string out;
    out.resize(bytes.size() * 2);
    for (size_t i = 0; i < bytes.size(); ++i) {
        out[i * 2] = kHex[(bytes[i] >> 4) & 0xF];
        out[i * 2 + 1] = kHex[bytes[i] & 0xF];
    }
    return out;
}

class EvpCtx final {
public:
    EvpCtx() : ctx_(EVP_MD_CTX_new()) {}
    EvpCtx(const EvpCtx&) = delete;
    EvpCtx& operator=(const EvpCtx&) = delete;
    ~EvpCtx() {
        if (ctx_) EVP_MD_CTX_free(ctx_);
    }

    EVP_MD_CTX* get() const { return ctx_; }
    bool ok() const { return ctx_ != nullptr; }

private:
    EVP_MD_CTX* ctx_ = nullptr;
};

bool InitSha256(EvpCtx& ctx) {
    return ctx.ok() && EVP_DigestInit_ex(ctx.get(), EVP_sha256(), nullptr) == 1;
}

bool UpdateSha256(EvpCtx& ctx, std::span<const std::uint8_t> data) {
    if (data.empty()) return true;
    return EVP_DigestUpdate(ctx.get(), data.data(), data.size()) == 1;
}

bool FinalSha256(EvpCtx& ctx, std::array<std::uint8_t, 32>& out) {
    unsigned int len = 0;
    if (EVP_DigestFinal_ex(ctx.get(), out.data(), &len) != 1) return false;
    return len == out.size();
}

} // namespace

struct Sha256Hasher::Impl {
    EvpCtx ctx;
    bool initialized = false;
    bool finalized = false;
};

Sha256Hasher::Sha256Hasher() : impl_(std::make_unique<Impl>()) {
    if (impl_ && InitSha256(impl_->ctx)) {
        impl_->initialized = true;
    }
}

Sha256Hasher::Sha256Hasher(Sha256Hasher&&) noexcept = default;
Sha256Hasher& Sha256Hasher::operator=(Sha256Hasher&&) noexcept = default;
Sha256Hasher::~Sha256Hasher() = default;

void Sha256Hasher::Update(std::span<const std::uint8_t> data) {
    if (!impl_ || !impl_->initialized || impl_->finalized) return;
    if (!UpdateSha256(impl_->ctx, data)) {
        impl_->finalized = true;
    }
}

std::string Sha256Hasher::FinalHex() {
    if (!impl_ || !impl_->initialized || impl_->finalized) return {};
    impl_->finalized = true;
    std::array<std::uint8_t, 32> digest{};
    if (!FinalSha256(impl_->ctx, digest)) return {};
    return HexEncode(digest);
}

std::string Sha256Hex(std::span<const std::uint8_t> data) {
    EvpCtx ctx;
    if (!InitSha256(ctx)) return {};
    if (!UpdateSha256(ctx, data)) return {};
    std::array<std::uint8_t, 32> digest{};
    if (!FinalSha256(ctx, digest)) return {};
    return HexEncode(digest);
}

std::string Sha256Hex(IReader& reader) {
    EvpCtx ctx;
    if (!InitSha256(ctx)) return {};

    std::vector<std::uint8_t> buf(64 * 1024);
    while (true) {
        const ssize_t n = reader.Read(std::span<std::uint8_t>(buf.data(), buf.size()));
        if (n == 0) break;
        if (n < 0) return {};
        if (!UpdateSha256(ctx, std::span<const std::uint8_t>(buf.data(), static_cast<size_t>(n)))) {
            return {};
        }
    }

    std::array<std::uint8_t, 32> digest{};
    if (!FinalSha256(ctx, digest)) return {};
    return HexEncode(digest);
}

Result Sha256HexFile(const std::string& path, std::string& out_hex) {
    FileOrStdinReader reader;
    auto r = FileOrStdinReader::Open(path, reader);
    if (!r.ok) return Result::Fail(-1, r.msg);
    out_hex = Sha256Hex(reader);
    if (out_hex.empty()) return Result::Fail(-1, "sha256 failed");
    return Result::Ok();
}

} // namespace flash
