#include "ota/mount_session.hpp"

#include <cerrno>
#include <cstring>
#include <filesystem>
#include <sys/mount.h>
#include <unistd.h>
#include <vector>

namespace fs = std::filesystem;

namespace flash {

MountSession::MountSession(MountSession&& other) noexcept
    : dir_(std::move(other.dir_)), mounted_(other.mounted_) {
    other.mounted_ = false;
}

MountSession& MountSession::operator=(MountSession&& other) noexcept {
    if (this == &other) return *this;
    Cleanup();
    dir_ = std::move(other.dir_);
    mounted_ = other.mounted_;
    other.mounted_ = false;
    return *this;
}

MountSession::~MountSession() {
    Cleanup();
}

Result MountSession::MountDevice(std::string_view device,
                                 std::string_view mount_base_dir,
                                 std::string_view mount_prefix,
                                 std::string_view fs_type,
                                 unsigned long mount_flags,
                                 MountSession& out) {
    out.Cleanup();

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

    out.dir_ = created;

    if (::mount(std::string(device).c_str(),
                out.dir_.c_str(),
                std::string(fs_type).c_str(),
                mount_flags,
                nullptr) != 0) {
        const int err = errno;
        out.Cleanup();
        return Result::Fail(err, "mount failed: " + std::string(std::strerror(err)));
    }

    out.mounted_ = true;
    return Result::Ok();
}

Result MountSession::Unmount() {
    if (!mounted_ || dir_.empty()) return Result::Ok();

    if (::umount2(dir_.c_str(), 0) != 0) {
        return Result::Fail(errno, "umount failed: " + std::string(std::strerror(errno)));
    }

    mounted_ = false;
    std::error_code ec;
    fs::remove(dir_, ec);
    dir_.clear();
    return Result::Ok();
}

void MountSession::Cleanup() {
    if (mounted_ && !dir_.empty()) {
        (void)::umount2(dir_.c_str(), 0);
    }
    mounted_ = false;
    if (!dir_.empty()) {
        std::error_code ec;
        fs::remove(dir_, ec);
        dir_.clear();
    }
}

} // namespace flash
