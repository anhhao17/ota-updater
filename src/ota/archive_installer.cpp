#include "ota/archive_installer.hpp"

#include "util/logger.hpp"
#include "ota/mount_session.hpp"
#include "util/path_utils.hpp"
#include "ota/tar_stream_extractor.hpp"

#include <filesystem>
#include <span>
#include <vector>
#include <sys/mount.h>

namespace fs = std::filesystem;

namespace flash {

ArchiveInstaller::ArchiveInstaller() : opt_() {
    opt_.mount_flags = MS_RELATIME;
}

ArchiveInstaller::ArchiveInstaller(Options opt) : opt_(std::move(opt)) {
    if (opt_.mount_flags == 0) opt_.mount_flags = MS_RELATIME;
}

Result ArchiveInstaller::InstallTarStreamToTarget(IReader& tar_stream,
                                                  std::string_view install_to,
                                                  std::string_view tag) {
    if (install_to.empty()) return Result::Fail(-1, "install_to is empty");

    TarStreamExtractor::Options xopt;
    xopt.progress = opt_.progress;
    xopt.progress_interval_bytes = opt_.progress_interval_bytes;
    xopt.safe_paths_only = opt_.safe_paths_only;
    xopt.progress_sink = opt_.progress_sink;
    xopt.component_total_bytes = opt_.component_total_bytes;
    xopt.overall_total_bytes = opt_.overall_total_bytes;
    xopt.overall_done_base_bytes = opt_.overall_done_base_bytes;
    TarStreamExtractor extractor(xopt);

    auto drain_stream = [&]() -> Result {
        std::vector<std::uint8_t> drain_buf(64 * 1024);
        while (true) {
            const ssize_t n = tar_stream.Read(std::span<std::uint8_t>(drain_buf.data(), drain_buf.size()));
            if (n == 0) break;
            if (n < 0) return Result::Fail(-1, "archive stream drain failed");
        }
        return Result::Ok();
    };

    if (IsDevPath(install_to)) {
        MountSession session;
        LogInfo("[%.*s] mount %.*s", (int)tag.size(), tag.data(), (int)install_to.size(), install_to.data());

        auto mr = MountSession::MountDevice(install_to,
                                            opt_.mount_base_dir,
                                            opt_.mount_prefix,
                                            opt_.fs_type,
                                            opt_.mount_flags,
                                            session);
        if (!mr.is_ok()) return mr;

        LogInfo("[%.*s] mounted at %s", (int)tag.size(), tag.data(), session.Dir().c_str());

        auto r = extractor.ExtractToDir(tar_stream, session.Dir(), tag);
        if (!r.is_ok()) return r;

        auto dr = drain_stream();
        if (!dr.is_ok()) return dr;

        auto ur = session.Unmount();
        if (!ur.is_ok()) return ur;

        LogInfo("[%.*s] archive install done", (int)tag.size(), tag.data());
        return Result::Ok();
    }

    fs::path dst(install_to);
    std::error_code ec;
    fs::create_directories(dst, ec);
    if (ec) {
        return Result::Fail(-1, "create_directories failed: " + dst.string() + ": " + ec.message());
    }

    LogInfo("[%.*s] extract -> %s", (int)tag.size(), tag.data(), dst.string().c_str());
    auto r = extractor.ExtractToDir(tar_stream, dst.string(), tag);
    if (!r.is_ok()) return r;

    auto dr = drain_stream();
    if (!dr.is_ok()) return dr;

    LogInfo("[%.*s] archive install done", (int)tag.size(), tag.data());
    return Result::Ok();
}

} // namespace flash
