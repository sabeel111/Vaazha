#include <filesystem>
#include <fstream>
#include <string>
#include <gtest/gtest.h>
#include "core/config/run_id.hpp"
#include "core/errors/agent_errors.hpp"
#include "policy/policy_guard.hpp"

namespace {

using agent::core::errors::get_error;
using agent::core::errors::get_value;
using agent::core::errors::is_error;
using agent::policy::PolicyGuard;

class TempWorkspace {
public:
    TempWorkspace() {
        root_ = std::filesystem::current_path() /
                (".tmp_policy_guard_" + agent::core::config::generate_run_id());
        std::filesystem::create_directories(root_ / "sub");
    }

    ~TempWorkspace() {
        std::error_code ec;
        std::filesystem::remove_all(root_, ec);
    }

    const std::filesystem::path& root() const { return root_; }

private:
    std::filesystem::path root_;
};

void write_file(const std::filesystem::path& path, const std::string& content) {
    std::filesystem::create_directories(path.parent_path());
    std::ofstream out(path);
    out << content;
}

TEST(PolicyGuardTest, AllowsPathInsideWorkspace) {
    TempWorkspace workspace;
    write_file(workspace.root() / "sub/sample.txt", "ok");

    PolicyGuard guard;
    auto result =
        guard.validate_path_in_workspace(workspace.root(), "sub/sample.txt");
    ASSERT_FALSE(is_error(result));

    const auto resolved = get_value(result);
    EXPECT_TRUE(resolved.is_absolute());
    EXPECT_EQ(resolved.filename().string(), "sample.txt");
}

TEST(PolicyGuardTest, RejectsPathOutsideWorkspace) {
    TempWorkspace workspace;
    const auto outside = workspace.root().parent_path() / "outside.txt";
    write_file(outside, "outside");

    PolicyGuard guard;
    auto result = guard.validate_path_in_workspace(workspace.root(), outside);
    ASSERT_TRUE(is_error(result));
    EXPECT_EQ(get_error(result).code, "path_outside_workspace");

    std::error_code ec;
    std::filesystem::remove(outside, ec);
}

TEST(PolicyGuardTest, RejectsInvalidWorkspaceRoot) {
    PolicyGuard guard;
    const auto missing_root =
        std::filesystem::current_path() /
        ("__missing_workspace_root__" + agent::core::config::generate_run_id());
    std::error_code ec;
    std::filesystem::remove_all(missing_root, ec);
    auto result = guard.validate_path_in_workspace(missing_root, "a.txt");
    ASSERT_TRUE(is_error(result));
    EXPECT_EQ(get_error(result).code, "invalid_workspace_root");
}

TEST(PolicyGuardTest, RejectsBlockedCommand) {
    PolicyGuard guard;
    auto result = guard.validate_command("sudo apt update");
    ASSERT_TRUE(is_error(result));
    EXPECT_EQ(get_error(result).code, "blocked_command");
}

TEST(PolicyGuardTest, RejectsBlockedCommandCaseInsensitive) {
    PolicyGuard guard;
    auto result = guard.validate_command("ReBoOt now");
    ASSERT_TRUE(is_error(result));
    EXPECT_EQ(get_error(result).code, "blocked_command");
}

TEST(PolicyGuardTest, AllowsSafeCommand) {
    PolicyGuard guard;
    auto result = guard.validate_command("rg RunManager src");
    ASSERT_FALSE(is_error(result));
    EXPECT_EQ(get_value(result), "rg RunManager src");
}

TEST(PolicyGuardTest, RejectsEmptyCommand) {
    PolicyGuard guard;
    auto result = guard.validate_command("");
    ASSERT_TRUE(is_error(result));
    EXPECT_EQ(get_error(result).code, "empty_command");
}

}  // namespace
