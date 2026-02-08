#include "ota/mount_session.hpp"

#include <cerrno>
#include <cstring>
#include <filesystem>
#include <memory>
#include <sys/mount.h>
#include <unistd.h>
#include <vector>

namespace fs = std::filesystem;

namespace flash {

namespace {

class PosixSystemOps final : public MountSession::ISystemOps {
  public:
    Result CreateMountPoint(std::string_view mount_base_dir,
                            std::string_view mount_prefix,
                            std::string& out_dir) const override {
        const fs::path base = mount_base_dir.empty() ? fs::path("/mnt") : fs::path(mount_base_dir);
        std::error_code ec;
        fs::create_directories(base, ec);

        std::string tmpl = (base / (std::string(mount_prefix) + "XXXXXX")).string();
        std::vector<char> buf(tmpl.begin(), tmpl.end());
        buf.push_back('\0');

        char* created = ::mkdtemp(buf.data());
        if (!created) {
            return Result::Fail(errno, "mkdtemp failed: " + std::string(std::strerror(errno)));
        }

        out_dir = created;
        return Result::Ok();
    }

    Result Mount(std::string_view device,
                 std::string_view target_dir,
                 std::string_view fs_type,
                 unsigned long mount_flags) const override {
        if (::mount(std::string(device).c_str(),
                    std::string(target_dir).c_str(),
                    std::string(fs_type).c_str(),
                    mount_flags,
                    nullptr) != 0) {
            const int err = errno;
            return Result::Fail(err, "mount failed: " + std::string(std::strerror(err)));
        }
        return Result::Ok();
    }

    Result Unmount(std::string_view target_dir) const override {
        if (::umount2(std::string(target_dir).c_str(), 0) != 0) {
            return Result::Fail(errno, "umount failed: " + std::string(std::strerror(errno)));
        }
        return Result::Ok();
    }

    void RemoveDirectory(std::string_view dir) const override {
        std::error_code ec;
        fs::remove(fs::path(dir), ec);
    }
};

} // namespace

std::shared_ptr<const MountSession::ISystemOps> MountSession::DefaultSystemOps() {
    static const std::shared_ptr<const ISystemOps> kDefault = std::make_shared<PosixSystemOps>();
    return kDefault;
}

MountSession::MountSession() : system_ops_(DefaultSystemOps()) {}

MountSession::MountSession(std::shared_ptr<const ISystemOps> system_ops)
    : system_ops_(system_ops ? std::move(system_ops) : DefaultSystemOps()) {}

MountSession::MountSession(MountSession&& other) noexcept
    : system_ops_(std::move(other.system_ops_)), dir_(std::move(other.dir_)),
      mounted_(other.mounted_) {
    other.mounted_ = false;
    other.dir_.clear();
    other.system_ops_ = DefaultSystemOps();
}

MountSession& MountSession::operator=(MountSession&& other) noexcept {
    if (this == &other)
        return *this;
    Cleanup();
    system_ops_ = std::move(other.system_ops_);
    dir_ = std::move(other.dir_);
    mounted_ = other.mounted_;
    other.mounted_ = false;
    other.dir_.clear();
    other.system_ops_ = DefaultSystemOps();
    return *this;
}

MountSession::~MountSession() { Cleanup(); }

Result MountSession::MountDevice(std::string_view device,
                                 std::string_view mount_base_dir,
                                 std::string_view mount_prefix,
                                 std::string_view fs_type,
                                 unsigned long mount_flags,
                                 MountSession& out) {
    out.Cleanup();

    auto create_result = out.system_ops_->CreateMountPoint(mount_base_dir, mount_prefix, out.dir_);
    if (!create_result.is_ok()) {
        out.dir_.clear();
        return create_result;
    }

    auto mount_result = out.system_ops_->Mount(device, out.dir_, fs_type, mount_flags);
    if (!mount_result.is_ok()) {
        out.Cleanup();
        return mount_result;
    }

    out.mounted_ = true;
    return Result::Ok();
}

Result MountSession::Unmount() {
    if (!mounted_ || dir_.empty())
        return Result::Ok();

    auto unmount_result = system_ops_->Unmount(dir_);
    if (!unmount_result.is_ok())
        return unmount_result;

    mounted_ = false;
    system_ops_->RemoveDirectory(dir_);
    dir_.clear();
    return Result::Ok();
}

void MountSession::Cleanup() {
    if (mounted_ && !dir_.empty()) {
        (void)system_ops_->Unmount(dir_);
    }
    mounted_ = false;
    if (!dir_.empty()) {
        system_ops_->RemoveDirectory(dir_);
        dir_.clear();
    }
}

} // namespace flash
