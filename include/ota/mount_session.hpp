#pragma once

#include "util/result.hpp"

#include <string>
#include <string_view>

namespace flash {

class MountSession {
public:
    MountSession() = default;
    MountSession(const MountSession&) = delete;
    MountSession& operator=(const MountSession&) = delete;
    MountSession(MountSession&& other) noexcept;
    MountSession& operator=(MountSession&& other) noexcept;
    ~MountSession();

    static Result MountDevice(std::string_view device,
                              std::string_view mount_base_dir,
                              std::string_view mount_prefix,
                              std::string_view fs_type,
                              unsigned long mount_flags,
                              MountSession& out);

    Result Unmount();
    const std::string& Dir() const { return dir_; }

private:
    void Cleanup();

    std::string dir_;
    bool mounted_ = false;
};

} // namespace flash
