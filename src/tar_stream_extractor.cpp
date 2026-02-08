#include "flash/tar_stream_extractor.hpp"

#include "flash/logger.hpp"
#include "flash/path_utils.hpp"
#include "flash/signals.hpp"

#include <archive.h>
#include <archive_entry.h>

#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <memory>
#include <unistd.h>
#include <vector>

namespace flash {

namespace {

struct ArchiveReadDeleter {
    void operator()(archive* a) const {
        if (a) archive_read_free(a);
    }
};

struct ArchiveWriteDeleter {
    void operator()(archive* a) const {
        if (a) archive_write_free(a);
    }
};

class ChdirGuard {
public:
    explicit ChdirGuard(const std::string& new_dir) {
        old_fd_ = ::open(".", O_RDONLY | O_DIRECTORY);
        if (old_fd_ < 0) return;

        if (::chdir(new_dir.c_str()) != 0) {
            ::close(old_fd_);
            old_fd_ = -1;
        }
    }

    ~ChdirGuard() {
        if (old_fd_ >= 0) {
            (void)::fchdir(old_fd_);
            ::close(old_fd_);
        }
    }

    bool ok() const { return old_fd_ >= 0; }

private:
    int old_fd_ = -1;
};

bool IsSafeRelativePath(const std::string& p) {
    if (p.empty()) return false;
    if (p.front() == '/') return false;
    if (p.find('\\') != std::string::npos) return false;

    std::string_view sv(p);
    while (!sv.empty()) {
        while (!sv.empty() && sv.front() == '/') sv.remove_prefix(1);
        const auto pos = sv.find('/');
        const auto seg = sv.substr(0, pos);
        if (seg == "..") return false;
        if (pos == std::string_view::npos) break;
        sv.remove_prefix(pos);
    }
    return true;
}

struct ReaderCtx {
    IReader* r = nullptr;
    std::vector<std::uint8_t> buf;

    explicit ReaderCtx(IReader& in, size_t buf_sz = 64 * 1024) : r(&in), buf(buf_sz) {}
};

la_ssize_t ReadCb(struct archive*, void* client_data, const void** out_buf) {
    if (flash::g_cancel.load(std::memory_order_relaxed)) {
        errno = EINTR;
        return -1;
    }

    auto* ctx = static_cast<ReaderCtx*>(client_data);
    const ssize_t n = ctx->r->Read(std::span<std::uint8_t>(ctx->buf.data(), ctx->buf.size()));
    if (n < 0) return -1;

    *out_buf = ctx->buf.data();
    return static_cast<la_ssize_t>(n);
}

int CloseCb(struct archive*, void* client_data) {
    delete static_cast<ReaderCtx*>(client_data);
    return ARCHIVE_OK;
}

std::string ArchiveErr(archive* a) {
    const char* s = a ? archive_error_string(a) : nullptr;
    return s ? std::string(s) : std::string("unknown");
}

} // namespace

Result TarStreamExtractor::ExtractToDir(IReader& tar_stream,
                                        const std::string& dst_dir,
                                        std::string_view tag) const {
    std::unique_ptr<archive, ArchiveReadDeleter> ar(archive_read_new());
    if (!ar) return Result::Fail(-1, "archive_read_new failed");

    archive_read_support_filter_all(ar.get());
    archive_read_support_format_all(ar.get());

    auto* ctx = new ReaderCtx(tar_stream);
    if (archive_read_open2(ar.get(), ctx, nullptr, ReadCb, nullptr, CloseCb) != ARCHIVE_OK) {
        delete ctx;
        return Result::Fail(-1, "archive_read_open2: " + ArchiveErr(ar.get()));
    }

    std::unique_ptr<archive, ArchiveWriteDeleter> aw(archive_write_disk_new());
    if (!aw) return Result::Fail(-1, "archive_write_disk_new failed");

    int flags = 0;
    flags |= ARCHIVE_EXTRACT_UNLINK;
    flags |= ARCHIVE_EXTRACT_PERM;
    flags |= ARCHIVE_EXTRACT_TIME;
    flags |= ARCHIVE_EXTRACT_SECURE_NODOTDOT;
    flags |= ARCHIVE_EXTRACT_SECURE_SYMLINKS;
#if defined(ARCHIVE_EXTRACT_SECURE_NOABSOLUTEPATHS)
    flags |= ARCHIVE_EXTRACT_SECURE_NOABSOLUTEPATHS;
#endif

    archive_write_disk_set_options(aw.get(), flags);
    archive_write_disk_set_standard_lookup(aw.get());

    ChdirGuard cd(dst_dir);
    if (!cd.ok()) {
        return Result::Fail(errno, "chdir(dst_dir) failed: " + std::string(std::strerror(errno)));
    }

    std::uint64_t extracted = 0;
    std::uint64_t next_progress =
        (opt_.progress_interval_bytes ? opt_.progress_interval_bytes : (4ULL * 1024 * 1024ULL));

    archive_entry* entry = nullptr;

    while (true) {
        const int r = archive_read_next_header(ar.get(), &entry);
        if (r == ARCHIVE_EOF) break;
        if (r != ARCHIVE_OK) return Result::Fail(-1, "archive_read_next_header: " + ArchiveErr(ar.get()));

        const char* p0 = archive_entry_pathname(entry);
        std::string rel = NormalizeTarPath(p0 ? std::string(p0) : std::string());

        if (rel.empty() || rel == ".") {
            (void)archive_read_data_skip(ar.get());
            continue;
        }
        if (opt_.safe_paths_only && !IsSafeRelativePath(rel)) {
            return Result::Fail(-1, "Unsafe path in archive: " + rel);
        }

        archive_entry_set_pathname(entry, rel.c_str());

        if (const char* hl0 = archive_entry_hardlink(entry); hl0 && *hl0) {
            std::string rel_hl = NormalizeTarPath(std::string(hl0));
            if (!rel_hl.empty() && rel_hl != ".") {
                if (opt_.safe_paths_only && !IsSafeRelativePath(rel_hl)) {
                    return Result::Fail(-1, "Unsafe hardlink target in archive: " + rel_hl);
                }
                archive_entry_set_hardlink(entry, rel_hl.c_str());
            }
        }

        LogDebug("[%.*s] entry: %s/%s", (int)tag.size(), tag.data(), dst_dir.c_str(), rel.c_str());

        const int wh = archive_write_header(aw.get(), entry);
        if (wh != ARCHIVE_OK) return Result::Fail(-1, "archive_write_header: " + ArchiveErr(aw.get()));

        const void* buff = nullptr;
        size_t size = 0;
        la_int64_t offset = 0;

        while (true) {
            const int rr = archive_read_data_block(ar.get(), &buff, &size, &offset);
            if (rr == ARCHIVE_EOF) break;
            if (rr != ARCHIVE_OK) return Result::Fail(-1, "archive_read_data_block: " + ArchiveErr(ar.get()));

            const int ww = archive_write_data_block(aw.get(), buff, size, offset);
            if (ww != ARCHIVE_OK) return Result::Fail(-1, "archive_write_data_block: " + ArchiveErr(aw.get()));

            extracted += static_cast<std::uint64_t>(size);
            if (opt_.progress && opt_.progress_interval_bytes > 0 && extracted >= next_progress) {
                LogInfo("[%.*s] extract progress: %llu bytes",
                        (int)tag.size(), tag.data(), (unsigned long long)extracted);
                next_progress = extracted + opt_.progress_interval_bytes;
            }
        }

        const int wf = archive_write_finish_entry(aw.get());
        if (wf != ARCHIVE_OK) return Result::Fail(-1, "archive_write_finish_entry: " + ArchiveErr(aw.get()));
    }

    return Result::Ok();
}

} // namespace flash
