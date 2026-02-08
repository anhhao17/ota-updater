#include "ota/tar_stream_extractor.hpp"

#include "ota/archive_path_policy.hpp"
#include "util/logger.hpp"
#include "ota/tar_stream_reader_adapter.hpp"

#include <archive.h>
#include <archive_entry.h>

#include <filesystem>
#include <memory>

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

} // namespace

Result TarStreamExtractor::ExtractToDir(IReader& tar_stream,
                                        const std::string& dst_dir,
                                        std::string_view tag) const {
    namespace fs = std::filesystem;

    const fs::path base_dir(dst_dir);

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
    // We rewrite entry paths to absolute paths under dst_dir, so NOABSOLUTEPATHS
    // would reject all valid extraction targets.

    archive_write_disk_set_options(aw.get(), flags);
    archive_write_disk_set_standard_lookup(aw.get());

    std::error_code ec;
    if (!fs::exists(base_dir, ec) || ec) {
        return Result::Fail(-1, "Destination directory does not exist: " + dst_dir);
    }
    if (!fs::is_directory(base_dir, ec) || ec) {
        return Result::Fail(-1, "Destination path is not a directory: " + dst_dir);
    }

    ArchivePathPolicy path_policy(opt_.safe_paths_only);
    const std::uint64_t declared_total = opt_.component_total_bytes;

    std::uint64_t extracted = 0;
    std::uint64_t next_progress =
        (opt_.progress_interval_bytes ? opt_.progress_interval_bytes : (4ULL * 1024 * 1024ULL));

    auto emit_progress = [&]() {
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
    };

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

        const std::string target_path = (base_dir / fs::path(rel)).string();
        archive_entry_set_pathname(entry, target_path.c_str());

        std::string rel_hl;
        auto hl_res = path_policy.NormalizeHardlinkPath(archive_entry_hardlink(entry), rel_hl);
        if (!hl_res.is_ok()) return hl_res;
        if (!rel_hl.empty() && rel_hl != ".") {
            const std::string hardlink_target = (base_dir / fs::path(rel_hl)).string();
            archive_entry_set_hardlink(entry, hardlink_target.c_str());
        }

        LogDebug("[%.*s] entry: %s", (int)tag.size(), tag.data(), target_path.c_str());

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
            emit_progress();
        }

        const int wf = archive_write_finish_entry(aw.get());
        if (wf != ARCHIVE_OK) return Result::Fail(-1, "archive_write_finish_entry: " + ArchiveErr(aw.get()));
    }

    if (opt_.progress_sink) {
        emit_progress();
    }

    return Result::Ok();
}

} // namespace flash
