#pragma once

#include "util/result.hpp"

#include <memory>
#include <string>
#include <string_view>

namespace flash {

class MountSession {
  public:
    class ISystemOps {
      public:
        virtual ~ISystemOps() = default;
        virtual Result CreateMountPoint(std::string_view mount_base_dir,
                                        std::string_view mount_prefix,
                                        std::string& out_dir) const = 0;
        virtual Result Mount(std::string_view device,
                             std::string_view target_dir,
                             std::string_view fs_type,
                             unsigned long mount_flags) const = 0;
        virtual Result Unmount(std::string_view target_dir) const = 0;
        virtual void RemoveDirectory(std::string_view dir) const = 0;
    };

    MountSession();
    explicit MountSession(std::shared_ptr<const ISystemOps> system_ops);
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

    static std::shared_ptr<const ISystemOps> DefaultSystemOps();

    std::shared_ptr<const ISystemOps> system_ops_;
    std::string dir_;
    bool mounted_ = false;
};

} // namespace flash
