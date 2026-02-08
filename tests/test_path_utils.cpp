#include <gtest/gtest.h>

#include "flash/path_utils.hpp"

TEST(PathUtilsTest, IsDevPath) {
    EXPECT_TRUE(flash::IsDevPath("/dev/mmcblk0p1"));
    EXPECT_TRUE(flash::IsDevPath("/dev/loop0"));
    EXPECT_FALSE(flash::IsDevPath("/device/mmcblk0p1"));
    EXPECT_FALSE(flash::IsDevPath("dev/mmcblk0p1"));
    EXPECT_FALSE(flash::IsDevPath("/tmp/dev/mmcblk0p1"));
}

TEST(PathUtilsTest, NormalizeTarPathCleansInput) {
    EXPECT_EQ(flash::NormalizeTarPath("./manifest.json"), "manifest.json");
    EXPECT_EQ(flash::NormalizeTarPath("/boot//EFI///BOOT"), "boot/EFI/BOOT");
    EXPECT_EQ(flash::NormalizeTarPath("////././a//b"), "././a/b");
    EXPECT_EQ(flash::NormalizeTarPath(""), "");
}
