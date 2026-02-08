#include <gtest/gtest.h>

#include "ota/archive_path_policy.hpp"

namespace flash {

TEST(ArchivePathPolicyTest, NormalizesSafeEntryPath) {
    ArchivePathPolicy policy(/*safe_paths_only=*/true);
    std::string out;

    auto res = policy.NormalizeEntryPath("./dir//file.txt", out);
    ASSERT_TRUE(res.is_ok()) << res.msg;
    EXPECT_EQ(out, "dir/file.txt");
}

TEST(ArchivePathPolicyTest, RejectsUnsafeEntryPath) {
    ArchivePathPolicy policy(/*safe_paths_only=*/true);
    std::string out;

    auto res = policy.NormalizeEntryPath("../escape.txt", out);
    ASSERT_FALSE(res.is_ok());
    EXPECT_NE(res.msg.find("Unsafe path in archive"), std::string::npos);
}

TEST(ArchivePathPolicyTest, RejectsUnsafeHardlinkTarget) {
    ArchivePathPolicy policy(/*safe_paths_only=*/true);
    std::string out;

    auto res = policy.NormalizeHardlinkPath("../etc/passwd", out);
    ASSERT_FALSE(res.is_ok());
    EXPECT_NE(res.msg.find("Unsafe hardlink target"), std::string::npos);
}

} // namespace flash
