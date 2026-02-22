#include "ota/update_module.hpp"
#include "testing.hpp"

#include <filesystem>
#include <fstream>
#include <gtest/gtest.h>
#include <memory>
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

TEST_F(UpdateModuleTest, ExecuteValidationFailures) {
    struct FailureCase {
        Component comp;
        std::string payload;
        std::string expected_error_substr;
        bool null_source = false;
    };

    std::vector<FailureCase> cases;
    {
        Component comp;
        comp.name = "cfg";
        comp.type = "file";
        comp.path = GetTestPath("config-invalid-perm.txt");
        comp.permissions = "invalid";
        cases.push_back({std::move(comp), "content", "Invalid permissions value", false});
    }
    {
        Component comp;
        comp.name = "x";
        comp.type = "unknown";
        cases.push_back({std::move(comp), "content", "Unsupported component type", false});
    }
    {
        Component comp;
        comp.name = "kernel";
        comp.type = "raw";
        cases.push_back({std::move(comp), "payload", "install_to empty", false});
    }
    {
        Component comp;
        comp.name = "cfg";
        comp.type = "file";
        comp.path = GetTestPath("config-null-reader.txt");
        cases.push_back({std::move(comp), "", "Null source reader", true});
    }

    for (const auto& c : cases) {
        std::unique_ptr<IReader> reader;
        if (!c.null_source) {
            reader = std::make_unique<testutil::MemoryReader>(c.payload);
        }

        Result res = UpdateModule::Execute(c.comp, std::move(reader));
        ASSERT_FALSE(res.is_ok());
        EXPECT_NE(res.msg.find(c.expected_error_substr), std::string::npos);
    }
}

} // namespace flash
