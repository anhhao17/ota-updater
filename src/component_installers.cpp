#include "flash/component_installers.hpp"

#include "flash/archive_installer.hpp"
#include "flash/logger.hpp"
#include "flash/partition_writer.hpp"
#include "flash/path_utils.hpp"

#include <cerrno>
#include <cstring>
#include <filesystem>
#include <optional>
#include <sys/stat.h>
#include <unistd.h>
#include <vector>

namespace flash {

namespace {

void EmitProgress(const UpdateModule::Options& opt,
                  const char* tag,
                  std::uint64_t in_done,
                  std::uint64_t out_written,
                  bool final) {
    if (opt.progress_sink) {
        ProgressEvent event{};
        event.component = tag;
        event.comp_done = in_done;
        event.comp_total = opt.component_total_bytes;
        event.overall_done = opt.overall_done_base_bytes + in_done;
        event.overall_total = opt.overall_total_bytes;
        opt.progress_sink->OnProgress(event);
    }

    if (!opt.progress) return;

    if (opt.component_total_bytes > 0) {
        const int comp_pct = static_cast<int>((in_done * 100ULL) / opt.component_total_bytes);

        if (opt.overall_total_bytes > 0) {
            const std::uint64_t overall_done = opt.overall_done_base_bytes + in_done;
            const int ota_pct = static_cast<int>((overall_done * 100ULL) / opt.overall_total_bytes);

            LogInfo("[%s] %s OTA:%d%% COMP:%d%% (in %llu/%llu, out %llu)",
                    tag,
                    final ? "done" : "progress",
                    ota_pct,
                    comp_pct,
                    (unsigned long long)in_done,
                    (unsigned long long)opt.component_total_bytes,
                    (unsigned long long)out_written);
        } else {
            LogInfo("[%s] %s %d%% (in %llu/%llu, out %llu)",
                    tag,
                    final ? "done" : "progress",
                    comp_pct,
                    (unsigned long long)in_done,
                    (unsigned long long)opt.component_total_bytes,
                    (unsigned long long)out_written);
        }
    } else {
        LogInfo("[%s] %s (in %llu bytes, out %llu bytes)",
                tag,
                final ? "done" : "progress",
                (unsigned long long)in_done,
                (unsigned long long)out_written);
    }
}

Result PipeReaderToWriter(IReader& r,
                          IWriter& w,
                          const UpdateModule::Options& opt,
                          const char* tag,
                          const std::uint64_t* in_read) {
    std::vector<std::uint8_t> buffer(1024 * 1024);

    std::uint64_t written = 0;
    std::uint64_t next_progress = opt.progress_interval_bytes;
    std::uint64_t next_fsync = opt.fsync_interval_bytes;

    EmitProgress(opt, tag, in_read ? *in_read : 0, written, false);

    while (true) {
        const ssize_t n = r.Read(std::span<std::uint8_t>(buffer.data(), buffer.size()));
        if (n == 0) break;
        if (n < 0) return Result::Fail(errno, "Read failed during pipe");

        auto res = w.WriteAll({buffer.data(), static_cast<size_t>(n)});
        if (!res.is_ok()) return res;

        written += static_cast<std::uint64_t>(n);

        const std::uint64_t in_done = in_read ? *in_read : written;

        if (opt.progress && opt.progress_interval_bytes > 0 && in_done >= next_progress) {
            EmitProgress(opt, tag, in_done, written, false);
            next_progress = in_done + opt.progress_interval_bytes;
        }

        if (opt.fsync_interval_bytes > 0 && written >= next_fsync) {
            auto fr = w.FsyncNow();
            if (!fr.is_ok()) return fr;
            LogDebug("[%s] fsync at out=%llu bytes", tag, (unsigned long long)written);
            next_fsync = written + opt.fsync_interval_bytes;
        }
    }

    auto fr = w.FsyncNow();
    if (!fr.is_ok()) return fr;

    EmitProgress(opt, tag, in_read ? *in_read : written, written, true);
    return Result::Ok();
}

Result ParsePermissions(const std::string& perm_str, mode_t& out_mode) {
    try {
        out_mode = static_cast<mode_t>(std::stoul(perm_str, nullptr, 8));
    } catch (const std::exception&) {
        return Result::Fail(-1, "Invalid permissions value: " + perm_str);
    }
    return Result::Ok();
}

std::string ResolveArchiveTarget(const Component& comp) {
    if (!comp.install_to.empty() && IsDevPath(comp.install_to)) {
        return comp.install_to;
    }
    if (!comp.path.empty()) {
        return comp.path;
    }
    if (!comp.install_to.empty()) {
        return comp.install_to;
    }
    return {};
}

class ProgressReader final : public IReader {
public:
    ProgressReader(IReader& inner,
                   const UpdateModule::Options& opt,
                   const char* tag,
                   const std::uint64_t* in_read)
        : inner_(inner),
          opt_(opt),
          tag_(tag),
          in_read_(in_read),
          next_progress_(opt.progress_interval_bytes) {}

    ssize_t Read(std::span<std::uint8_t> out) override {
        const ssize_t n = inner_.Read(out);
        if (n <= 0) return n;

        local_read_ += static_cast<std::uint64_t>(n);
        const std::uint64_t in_done = in_read_ ? *in_read_ : local_read_;

        if ((opt_.progress || opt_.progress_sink) && opt_.progress_interval_bytes > 0) {
            if (in_done >= next_progress_) {
                EmitProgress(opt_, tag_, in_done, in_done, false);
                next_progress_ = in_done + opt_.progress_interval_bytes;
            }
        }

        return n;
    }

    std::optional<std::uint64_t> TotalSize() const override {
        return inner_.TotalSize();
    }

private:
    IReader& inner_;
    const UpdateModule::Options& opt_;
    const char* tag_ = nullptr;
    const std::uint64_t* in_read_ = nullptr;
    std::uint64_t local_read_ = 0;
    std::uint64_t next_progress_ = 0;
};

class RawInstallerStrategy final : public UpdateModule::IInstallerStrategy {
public:
    bool Supports(const Component& comp) const override {
        return comp.type == "raw";
    }

    Result Install(const Component& comp,
                   IReader& reader,
                   const UpdateModule::Options& opt,
                   const char* tag,
                   const std::uint64_t* in_read) const override {
        if (comp.install_to.empty()) {
            return Result::Fail(-1, "install_to empty for raw component: " + comp.name);
        }

        PartitionWriter writer;
        auto open_res = PartitionWriter::Open(comp.install_to, writer);
        if (!open_res.is_ok()) return open_res;

        return PipeReaderToWriter(reader, writer, opt, tag, in_read);
    }
};

class ArchiveInstallerStrategy final : public UpdateModule::IInstallerStrategy {
public:
    bool Supports(const Component& comp) const override {
        return comp.type == "archive";
    }

    Result Install(const Component& comp,
                   IReader& reader,
                   const UpdateModule::Options& opt,
                   const char* tag,
                   const std::uint64_t* in_read) const override {
        std::string target = ResolveArchiveTarget(comp);
        if (target.empty()) {
            return Result::Fail(-1, "archive component needs install_to(/dev/...) or path(folder): " + comp.name);
        }

        ArchiveInstaller::Options aopt;
        aopt.progress = opt.progress;
        aopt.progress_interval_bytes = opt.progress_interval_bytes;

        ArchiveInstaller installer(aopt);
        ProgressReader progress_reader(reader, opt, tag, in_read);
        return installer.InstallTarStreamToTarget(progress_reader, target, comp.name);
    }
};

class AtomicFileInstallerStrategy final : public UpdateModule::IInstallerStrategy {
public:
    bool Supports(const Component& comp) const override {
        return comp.type == "file";
    }

    Result Install(const Component& comp,
                   IReader& reader,
                   const UpdateModule::Options& opt,
                   const char* tag,
                   const std::uint64_t* in_read) const override {
        if (comp.path.empty()) {
            return Result::Fail(-1, "File path is empty for component: " + comp.name);
        }

        namespace fs = std::filesystem;
        fs::path dst(comp.path);
        fs::path parent = dst.parent_path();

        if (!parent.empty() && !fs::exists(parent)) {
            if (!comp.create_destination) {
                return Result::Fail(ENOENT,
                    "Destination directory does not exist: " + parent.string() +
                    " (set 'create-destination': true in manifest)");
            }

            std::error_code ec;
            fs::create_directories(parent, ec);
            if (ec) {
                return Result::Fail(-1, "create_directories failed: " + parent.string() + ": " + ec.message());
            }
            LogInfo("Created destination directory: %s", parent.string().c_str());
        }

        std::string tmp_path = comp.path + ".tmp";

        PartitionWriter writer;
        auto open_res = PartitionWriter::Open(tmp_path, writer);
        if (!open_res.is_ok()) return open_res;

        auto pipe_res = PipeReaderToWriter(reader, writer, opt, tag, in_read);
        if (!pipe_res.is_ok()) {
            ::unlink(tmp_path.c_str());
            return pipe_res;
        }

        if (::rename(tmp_path.c_str(), comp.path.c_str()) != 0) {
            const int err = errno;
            ::unlink(tmp_path.c_str());
            return Result::Fail(err, "Atomic rename failed: " + std::string(std::strerror(err)));
        }

        if (!comp.permissions.empty()) {
            mode_t mode = 0;
            auto perm_res = ParsePermissions(comp.permissions, mode);
            if (!perm_res.is_ok()) {
                return perm_res;
            }
            if (::chmod(comp.path.c_str(), mode) != 0) {
                const int err = errno;
                return Result::Fail(err, "chmod failed: " + std::string(std::strerror(err)));
            }
        }

        return Result::Ok();
    }
};

} // namespace

std::vector<std::unique_ptr<UpdateModule::IInstallerStrategy>> CreateDefaultInstallerStrategies() {
    std::vector<std::unique_ptr<UpdateModule::IInstallerStrategy>> out;
    out.emplace_back(std::make_unique<RawInstallerStrategy>());
    out.emplace_back(std::make_unique<ArchiveInstallerStrategy>());
    out.emplace_back(std::make_unique<AtomicFileInstallerStrategy>());
    return out;
}

} // namespace flash
