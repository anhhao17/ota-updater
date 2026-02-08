#include "flash/tar_stream_extractor.hpp"

#include "flash/archive_path_policy.hpp"
#include "flash/logger.hpp"
#include "flash/tar_stream_reader_adapter.hpp"

#include <archive.h>
#include <archive_entry.h>

#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <memory>
#include <unistd.h>

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

} // namespace

Result TarStreamExtractor::ExtractToDir(IReader& tar_stream,
                                        const std::string& dst_dir,
                                        std::string_view tag) const {
    std::unique_ptr<archive, ArchiveReadDeleter> ar(archive_read_new());
    if (!ar) return Result::Fail(-1, "archive_read_new failed");

    archive_read_support_filter_all(ar.get());
    archive_read_support_format_all(ar.get());

    if (OpenArchiveFromReader(ar.get(), tar_stream) != ARCHIVE_OK) {
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

    ArchivePathPolicy path_policy(opt_.safe_paths_only);
    const std::uint64_t declared_total = opt_.component_total_bytes;

    std::uint64_t extracted = 0;
    std::uint64_t next_progress =
        (opt_.progress_interval_bytes ? opt_.progress_interval_bytes : (4ULL * 1024 * 1024ULL));

    archive_entry* entry = nullptr;

    while (true) {
        const int r = archive_read_next_header(ar.get(), &entry);
        if (r == ARCHIVE_EOF) break;
        if (r != ARCHIVE_OK) return Result::Fail(-1, "archive_read_next_header: " + ArchiveErr(ar.get()));

        std::string rel;
        auto path_res = path_policy.NormalizeEntryPath(archive_entry_pathname(entry), rel);
        if (!path_res.is_ok()) return path_res;
        if (rel.empty() || rel == ".") {
            (void)archive_read_data_skip(ar.get());
            continue;
        }

        archive_entry_set_pathname(entry, rel.c_str());

        std::string rel_hl;
        auto hl_res = path_policy.NormalizeHardlinkPath(archive_entry_hardlink(entry), rel_hl);
        if (!hl_res.is_ok()) return hl_res;
        if (!rel_hl.empty() && rel_hl != ".") {
            archive_entry_set_hardlink(entry, rel_hl.c_str());
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
            if (opt_.progress_sink) {
                ProgressEvent event{};
                event.component = tag;
                event.comp_done = extracted;
                event.comp_total = declared_total;
                event.overall_done = opt_.overall_done_base_bytes + extracted;
                event.overall_total = opt_.overall_total_bytes;
                opt_.progress_sink->OnProgress(event);
            } else if (opt_.progress && opt_.progress_interval_bytes > 0 && extracted >= next_progress) {
                LogInfo("[%.*s] extract progress: %llu bytes",
                        (int)tag.size(), tag.data(), (unsigned long long)extracted);
                next_progress = extracted + opt_.progress_interval_bytes;
            }
        }

        const int wf = archive_write_finish_entry(aw.get());
        if (wf != ARCHIVE_OK) return Result::Fail(-1, "archive_write_finish_entry: " + ArchiveErr(aw.get()));
    }

    if (opt_.progress_sink) {
        ProgressEvent event{};
        event.component = tag;
        event.comp_done = extracted;
        event.comp_total = declared_total;
        event.overall_done = opt_.overall_done_base_bytes + extracted;
        event.overall_total = opt_.overall_total_bytes;
        opt_.progress_sink->OnProgress(event);
    }

    return Result::Ok();
}

} // namespace flash
