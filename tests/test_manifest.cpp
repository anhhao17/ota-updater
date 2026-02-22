#include "util/manifest.hpp"

#include <gtest/gtest.h>

using namespace flash;

TEST(ManifestTest, VersionComparisonLogic) {
    struct CompareCase {
        const char* lhs;
        const char* rhs;
        int expected;
    };
    const CompareCase cases[] = {
        {"1.2.3", "1.2.2", 1},
        {"1.0.0", "2.0.0", -1},
        {"1.2", "1.2.0", 0},
        {"1.2.1", "1.2", 1},
        {"2", "1.9.9", 1},
        {"1.2.a", "1.2.0", 0},
        {"", "", 0},
    };
    for (const auto& c : cases) {
        EXPECT_EQ(ManifestHandler::CompareVersions(c.lhs, c.rhs), c.expected);
    }
}

TEST(ManifestTest, ShouldUpdateLogic) {
    Manifest m;
    m.hw_compatibility = "orin-nano";

    Component c;
    c.name = "kernel";
    c.version = "2.0.0";

    EXPECT_TRUE(ManifestHandler::ShouldUpdate(c, m, "1.9.0"));

    EXPECT_FALSE(ManifestHandler::ShouldUpdate(c, m, "2.1.0"));

    EXPECT_FALSE(ManifestHandler::ShouldUpdate(c, m, "2.0.0"));

    c.force = true;
    EXPECT_TRUE(ManifestHandler::ShouldUpdate(c, m, "2.1.0"));
}

TEST(ManifestTest, ParserEdgeCases) {
    {
        std::string bad_json = R"({"something": "invalid"})";
        auto res = ManifestHandler::Parse(bad_json);
        EXPECT_TRUE(res->components.empty());
    }

    struct ParseFailCase {
        std::string json;
        std::string expected_error_substr;
    };
    const std::vector<ParseFailCase> fail_cases = {
        {"not json at all", "Syntax Error"},
        {R"({
        "version": 1.0, 
        "components": [{"name": "test", "version": 2}]
    })",
         "type must be string"},
        {R"({
        "version":"1.0.0",
        "components":[{"name":"kernel","type":"raw","filename":"kernel.bin"}]
    })",
         "missing sha256"},
    };
    for (const auto& c : fail_cases) {
        auto res = ManifestHandler::Parse(c.json);
        EXPECT_FALSE(res.has_value());
        EXPECT_NE(res.error().find(c.expected_error_substr), std::string::npos);
    }
}

TEST(ManifestTest, PlanningLogicWithForce) {
    Manifest m;
    m.force_all = true;
    Component c;
    c.version = "0.0.1"; // Older than system

    EXPECT_TRUE(ManifestHandler::ShouldUpdate(c, m, "1.0.0"));
    m.force_all = false;
    EXPECT_FALSE(ManifestHandler::ShouldUpdate(c, m, "1.0.0"));
}

TEST(ManifestTest, FullParseTest) {
    std::string raw =
        R"({"version":"1.0","components":[{"name":"boot","version":"1.1","sha256":"abcd"}]})";
    auto m = ManifestHandler::Parse(raw);
    ASSERT_TRUE(m.has_value());
    EXPECT_EQ(m->components[0].version, "1.1");
}
