#include "ota/update_module.hpp"
#include "testing.hpp"

#include <filesystem>
#include <fstream>
#include <gtest/gtest.h>
#include <string>
#include <vector>

namespace flash {

class UpdateModuleTest : public ::testing::Test {
  protected:
    testutil::TemporaryDirectory temp_dir;

    std::string GetTestPath(const std::string& filename) {
        return temp_dir.Path() + "/" + filename;
    }
};

TEST_F(UpdateModuleTest, ExecuteAtomicFile) {
    UpdateModule module;
    std::string target_path = GetTestPath("config.txt");

    Component comp;
    comp.name = "test_config";
    comp.type = "file";
    comp.path = target_path;
    comp.permissions = "0600";

    std::string expected_content = "SECRET_KEY=12345";
    auto reader = std::make_unique<testutil::MemoryReader>(expected_content);

    // Call the public entry point
    Result res = module.Execute(comp, std::move(reader));

    ASSERT_TRUE(res.is_ok()) << "Execute failed: " << res.msg;

    // Verify file exists and matches
    std::ifstream ifs(target_path);
    std::string actual((std::istreambuf_iterator<char>(ifs)), std::istreambuf_iterator<char>());
    EXPECT_EQ(actual, expected_content);

    // Verify permissions
    struct stat st;
    stat(target_path.c_str(), &st);
    EXPECT_EQ(st.st_mode & 0777, 0600);
}

TEST_F(UpdateModuleTest, ExecuteGzippedRaw) {
    UpdateModule module;
    std::string partition_path = GetTestPath("fake_part");

    Component comp;
    comp.name = "kernel";
    comp.type = "raw";
    comp.filename = "image.gz";
    comp.install_to = partition_path;

    std::vector<uint8_t> gz_data = {0x1f, 0x8b, 0x08, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                                    0x03, 0xcb, 0x48, 0xcd, 0xc9, 0xc9, 0x07, 0x00, 0x86,
                                    0xa6, 0x10, 0x36, 0x05, 0x00, 0x00, 0x00};

    auto reader = std::make_unique<testutil::MemoryReader>(std::move(gz_data));

    Result res = module.Execute(comp, std::move(reader));

    ASSERT_TRUE(res.is_ok()) << "Execute failed: " << res.msg;

    std::ifstream ifs(partition_path);
    std::string actual;
    ifs >> actual;
    EXPECT_EQ(actual, "hello");
}

TEST_F(UpdateModuleTest, ExecuteAtomicFile_MissingDirectoryWithoutCreateDestination_Fails) {
    Component comp;
    comp.name = "cfg";
    comp.type = "file";
    comp.path = GetTestPath("missing/dir/config.txt");
    comp.create_destination = false;

    auto reader = std::make_unique<testutil::MemoryReader>("hello");
    Result res = UpdateModule::Execute(comp, std::move(reader));
    ASSERT_FALSE(res.is_ok());
    EXPECT_NE(res.msg.find("Destination directory does not exist"), std::string::npos);
}

TEST_F(UpdateModuleTest, ExecuteAtomicFile_CreateDestination_Succeeds) {
    Component comp;
    comp.name = "cfg";
    comp.type = "file";
    comp.path = GetTestPath("new/dir/config.txt");
    comp.create_destination = true;
    comp.permissions = "0640";

    auto reader = std::make_unique<testutil::MemoryReader>("key=value");
    Result res = UpdateModule::Execute(comp, std::move(reader));
    ASSERT_TRUE(res.is_ok()) << res.msg;

    namespace fs = std::filesystem;
    EXPECT_TRUE(fs::exists(fs::path(comp.path).parent_path()));
    EXPECT_TRUE(fs::exists(comp.path));
}

TEST_F(UpdateModuleTest, ExecuteAtomicFile_InvalidPermissions_Fails) {
    Component comp;
    comp.name = "cfg";
    comp.type = "file";
    comp.path = GetTestPath("config.txt");
    comp.permissions = "invalid";

    auto reader = std::make_unique<testutil::MemoryReader>("content");
    Result res = UpdateModule::Execute(comp, std::move(reader));
    ASSERT_FALSE(res.is_ok());
    EXPECT_NE(res.msg.find("Invalid permissions value"), std::string::npos);
}

TEST_F(UpdateModuleTest, ExecuteUnsupportedType_Fails) {
    Component comp;
    comp.name = "x";
    comp.type = "unknown";

    auto reader = std::make_unique<testutil::MemoryReader>("content");
    Result res = UpdateModule::Execute(comp, std::move(reader));
    ASSERT_FALSE(res.is_ok());
    EXPECT_NE(res.msg.find("Unsupported component type"), std::string::npos);
}

TEST_F(UpdateModuleTest, ExecuteRawWithoutTarget_Fails) {
    Component comp;
    comp.name = "kernel";
    comp.type = "raw";
    comp.install_to.clear();

    auto reader = std::make_unique<testutil::MemoryReader>("payload");
    Result res = UpdateModule::Execute(comp, std::move(reader));
    ASSERT_FALSE(res.is_ok());
    EXPECT_NE(res.msg.find("install_to empty"), std::string::npos);
}

TEST_F(UpdateModuleTest, ExecuteNullSourceReader_Fails) {
    Component comp;
    comp.name = "cfg";
    comp.type = "file";
    comp.path = GetTestPath("config.txt");

    std::unique_ptr<IReader> null_reader;
    Result res = UpdateModule::Execute(comp, std::move(null_reader));
    ASSERT_FALSE(res.is_ok());
    EXPECT_NE(res.msg.find("Null source reader"), std::string::npos);
}

} // namespace flash
