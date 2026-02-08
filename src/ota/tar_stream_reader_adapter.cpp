#include "ota/tar_stream_reader_adapter.hpp"

#include "system/signals.hpp"

#include <cerrno>
#include <vector>

namespace flash {

namespace {

struct ReaderCtx {
    IReader* reader = nullptr;
    std::vector<std::uint8_t> buffer;

    explicit ReaderCtx(IReader& in, size_t buffer_size = 64 * 1024)
        : reader(&in), buffer(buffer_size) {}
};

la_ssize_t ReadCb(struct archive*, void* client_data, const void** out_buf) {
    if (g_cancel.load(std::memory_order_relaxed)) {
        errno = EINTR;
        return -1;
    }

    auto* ctx = static_cast<ReaderCtx*>(client_data);
    const ssize_t n = ctx->reader->Read(std::span<std::uint8_t>(ctx->buffer.data(), ctx->buffer.size()));
    if (n < 0) return -1;

    *out_buf = ctx->buffer.data();
    return static_cast<la_ssize_t>(n);
}

int CloseCb(struct archive*, void* client_data) {
    delete static_cast<ReaderCtx*>(client_data);
    return ARCHIVE_OK;
}

} // namespace

int OpenArchiveFromReader(struct archive* ar, IReader& reader) {
    auto* ctx = new ReaderCtx(reader);
    const int rc = archive_read_open2(ar, ctx, nullptr, ReadCb, nullptr, CloseCb);
    if (rc != ARCHIVE_OK) {
        delete ctx;
    }
    return rc;
}

std::string ArchiveErr(struct archive* ar) {
    const char* s = ar ? archive_error_string(ar) : nullptr;
    return s ? std::string(s) : std::string("unknown");
}

} // namespace flash
