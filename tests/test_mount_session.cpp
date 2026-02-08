#include <gtest/gtest.h>

#include "ota/mount_session.hpp"

#include <memory>
#include <string>
#include <string_view>

namespace flash {
namespace {

class FakeSystemOps final : public MountSession::ISystemOps {
public:
    Result create_result = Result::Ok();
    Result mount_result = Result::Ok();
    Result unmount_result = Result::Ok();
    std::string created_dir = "/tmp/fake-mount";

    mutable int create_calls = 0;
    mutable int mount_calls = 0;
    mutable int unmount_calls = 0;
    mutable int remove_calls = 0;

    Result CreateMountPoint(std::string_view, std::string_view, std::string& out_dir) const override {
        ++create_calls;
        if (!create_result.is_ok()) return create_result;
        out_dir = created_dir;
        return Result::Ok();
    }

    Result Mount(std::string_view, std::string_view, std::string_view, unsigned long) const override {
        ++mount_calls;
        return mount_result;
    }

    Result Unmount(std::string_view) const override {
        ++unmount_calls;
        return unmount_result;
    }

    void RemoveDirectory(std::string_view) const override {
        ++remove_calls;
    }
};

TEST(MountSessionTest, MountAndUnmountSuccess) {
    auto ops = std::make_shared<FakeSystemOps>();
    MountSession session(ops);

    auto res = MountSession::MountDevice("/dev/mock", "/mnt", "ota-", "ext4", 0UL, session);
    ASSERT_TRUE(res.is_ok()) << res.msg;
    EXPECT_EQ(session.Dir(), ops->created_dir);
    EXPECT_EQ(ops->create_calls, 1);
    EXPECT_EQ(ops->mount_calls, 1);

    auto unmount_res = session.Unmount();
    ASSERT_TRUE(unmount_res.is_ok()) << unmount_res.msg;
    EXPECT_TRUE(session.Dir().empty());
    EXPECT_EQ(ops->unmount_calls, 1);
    EXPECT_EQ(ops->remove_calls, 1);
}

TEST(MountSessionTest, MountFailureCleansMountPoint) {
    auto ops = std::make_shared<FakeSystemOps>();
    ops->mount_result = Result::Fail(5, "mount failed");
    MountSession session(ops);

    auto res = MountSession::MountDevice("/dev/mock", "/mnt", "ota-", "ext4", 0UL, session);
    ASSERT_FALSE(res.is_ok());
    EXPECT_EQ(res.msg, "mount failed");
    EXPECT_TRUE(session.Dir().empty());
    EXPECT_EQ(ops->create_calls, 1);
    EXPECT_EQ(ops->mount_calls, 1);
    EXPECT_EQ(ops->unmount_calls, 0);
    EXPECT_EQ(ops->remove_calls, 1);
}

TEST(MountSessionTest, UnmountFailureKeepsState) {
    auto ops = std::make_shared<FakeSystemOps>();
    MountSession session(ops);
    ASSERT_TRUE(MountSession::MountDevice("/dev/mock", "/mnt", "ota-", "ext4", 0UL, session).is_ok());

    ops->unmount_result = Result::Fail(16, "busy");
    auto unmount_res = session.Unmount();
    ASSERT_FALSE(unmount_res.is_ok());
    EXPECT_EQ(unmount_res.msg, "busy");
    EXPECT_EQ(session.Dir(), ops->created_dir);
    EXPECT_EQ(ops->unmount_calls, 1);
    EXPECT_EQ(ops->remove_calls, 0);
}

TEST(MountSessionTest, CreateMountPointFailureIsPropagated) {
    auto ops = std::make_shared<FakeSystemOps>();
    ops->create_result = Result::Fail(2, "mkdtemp failed");
    MountSession session(ops);

    auto res = MountSession::MountDevice("/dev/mock", "/mnt", "ota-", "ext4", 0UL, session);
    ASSERT_FALSE(res.is_ok());
    EXPECT_EQ(res.msg, "mkdtemp failed");
    EXPECT_TRUE(session.Dir().empty());
    EXPECT_EQ(ops->create_calls, 1);
    EXPECT_EQ(ops->mount_calls, 0);
    EXPECT_EQ(ops->remove_calls, 0);
}

TEST(MountSessionTest, MoveTransfersActiveSession) {
    auto ops = std::make_shared<FakeSystemOps>();
    MountSession original(ops);
    ASSERT_TRUE(MountSession::MountDevice("/dev/mock", "/mnt", "ota-", "ext4", 0UL, original).is_ok());

    MountSession moved(std::move(original));
    EXPECT_EQ(moved.Dir(), ops->created_dir);

    auto unmount_res = moved.Unmount();
    ASSERT_TRUE(unmount_res.is_ok()) << unmount_res.msg;
    EXPECT_EQ(ops->unmount_calls, 1);
    EXPECT_EQ(ops->remove_calls, 1);
}

} // namespace
} // namespace flash
