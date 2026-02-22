#include <atomic>
#include <filesystem>
#include <fstream>
#include <string>
#include <gtest/gtest.h>
#include "core/config/run_id.hpp"
#include "core/errors/agent_errors.hpp"
#include "tools/tool_host.hpp"

namespace {

using agent::core::errors::get_error;
using agent::core::errors::get_value;
using agent::core::errors::is_error;
using agent::tools::SearchRequest;
using agent::tools::ToolHost;

class TempWorkspace {
public:
    TempWorkspace() {
        root_ = std::filesystem::current_path() /
                (".tmp_tool_host_" + agent::core::config::generate_run_id());
        std::filesystem::create_directories(root_);
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

TEST(ToolHostTest, ReadFileReturnsContent) {
    TempWorkspace workspace;
    const auto file = workspace.root() / "notes.txt";
    write_file(file, "hello tool host");

    ToolHost host;
    auto result = host.read_file(workspace.root(), "notes.txt");
    ASSERT_FALSE(is_error(result));

    const auto& tool_result = get_value(result);
    EXPECT_TRUE(tool_result.success);
    EXPECT_EQ(tool_result.output, "hello tool host");
    EXPECT_TRUE(tool_result.error_message.empty());
    EXPECT_EQ(tool_result.tool_call_id, "read_file");
}

TEST(ToolHostTest, ReadFileRejectsPathOutsideWorkspace) {
    TempWorkspace workspace;
    const auto outside = workspace.root().parent_path() / "outside.txt";
    write_file(outside, "outside");

    ToolHost host;
    auto result = host.read_file(workspace.root(), outside);
    ASSERT_TRUE(is_error(result));
    EXPECT_EQ(get_error(result).code, "path_outside_workspace");

    std::error_code ec;
    std::filesystem::remove(outside, ec);
}

TEST(ToolHostTest, SearchFindsMatchesRecursively) {
    TempWorkspace workspace;
    write_file(workspace.root() / "a.cpp", "int needle = 1;\n");
    write_file(workspace.root() / "sub/b.cpp", "needle and more needle\n");
    write_file(workspace.root() / "sub/c.cpp", "no match here\n");

    ToolHost host;
    SearchRequest request;
    request.pattern = "needle";
    request.scope = ".";
    request.max_matches = 4;

    auto result = host.search(workspace.root(), request);
    ASSERT_FALSE(is_error(result));
    const auto& tool_result = get_value(result);
    EXPECT_TRUE(tool_result.success);
    EXPECT_NE(tool_result.output.find("matches="), std::string::npos);
    EXPECT_NE(tool_result.output.find("a.cpp:1"), std::string::npos);
    EXPECT_NE(tool_result.output.find("b.cpp:1"), std::string::npos);
    EXPECT_EQ(tool_result.tool_call_id, "search");
}

TEST(ToolHostTest, SearchReportsNoMatches) {
    TempWorkspace workspace;
    write_file(workspace.root() / "x.txt", "alpha beta gamma\n");

    ToolHost host;
    SearchRequest request;
    request.pattern = "needle";
    request.scope = ".";

    auto result = host.search(workspace.root(), request);
    ASSERT_FALSE(is_error(result));
    const auto& tool_result = get_value(result);
    EXPECT_TRUE(tool_result.success);
    EXPECT_NE(tool_result.output.find("matches=0"), std::string::npos);
    EXPECT_NE(tool_result.output.find("No matches found."), std::string::npos);
}

TEST(ToolHostTest, SearchRejectsEmptyPattern) {
    TempWorkspace workspace;
    ToolHost host;
    SearchRequest request;
    request.pattern = "";
    request.scope = ".";

    auto result = host.search(workspace.root(), request);
    ASSERT_TRUE(is_error(result));
    EXPECT_EQ(get_error(result).code, "empty_search_pattern");
}

TEST(ToolHostTest, RunCommandExecutesSuccessfully) {
    TempWorkspace workspace;
    ToolHost host;

    agent::tools::CommandRequest request;
    request.command = "printf 'hello'";
    request.timeout_ms = 1000;
    request.working_directory = ".";

    auto result = host.run_command(workspace.root(), request);
    ASSERT_FALSE(is_error(result));
    const auto& tool_result = get_value(result);
    EXPECT_TRUE(tool_result.success);
    EXPECT_EQ(tool_result.tool_call_id, "run_command");
    EXPECT_EQ(tool_result.output, "hello");
}

TEST(ToolHostTest, RunCommandRejectsBlockedOperation) {
    TempWorkspace workspace;
    ToolHost host;

    agent::tools::CommandRequest request;
    request.command = "sudo ls";
    request.timeout_ms = 1000;

    auto result = host.run_command(workspace.root(), request);
    ASSERT_TRUE(is_error(result));
    EXPECT_EQ(get_error(result).code, "blocked_command");
}

TEST(ToolHostTest, RunCommandTimesOut) {
    TempWorkspace workspace;
    ToolHost host;

    agent::tools::CommandRequest request;
    request.command = "sleep 1";
    request.timeout_ms = 30;

    auto result = host.run_command(workspace.root(), request);
    ASSERT_FALSE(is_error(result));
    const auto& tool_result = get_value(result);
    EXPECT_FALSE(tool_result.success);
    EXPECT_NE(tool_result.error_message.find("timed out"), std::string::npos);
}

TEST(ToolHostTest, RunCommandHonorsCancellationToken) {
    TempWorkspace workspace;
    ToolHost host;

    auto token = std::make_shared<std::atomic_bool>(true);
    agent::tools::CommandRequest request;
    request.command = "sleep 1";
    request.timeout_ms = 1000;
    request.cancel_token = token;

    auto result = host.run_command(workspace.root(), request);
    ASSERT_FALSE(is_error(result));
    const auto& tool_result = get_value(result);
    EXPECT_FALSE(tool_result.success);
    EXPECT_NE(tool_result.error_message.find("cancelled"), std::string::npos);
}

TEST(ToolHostTest, ApplyPatchUpdatesFile) {
    TempWorkspace workspace;
    const auto target = workspace.root() / "file.txt";
    write_file(target, "old\n");

    ToolHost host;
    agent::tools::PatchRequest request;
    request.patch_text =
        "diff --git a/file.txt b/file.txt\n"
        "--- a/file.txt\n"
        "+++ b/file.txt\n"
        "@@ -1 +1 @@\n"
        "-old\n"
        "+new\n";
    request.timeout_ms = 1000;

    auto result = host.apply_patch(workspace.root(), request);
    ASSERT_FALSE(is_error(result));
    const auto& tool_result = get_value(result);
    EXPECT_TRUE(tool_result.success);
    EXPECT_EQ(tool_result.tool_call_id, "apply_patch");

    std::ifstream in(target);
    std::string content;
    std::getline(in, content);
    EXPECT_EQ(content, "new");
}

TEST(ToolHostTest, ApplyPatchRejectsInvalidFormat) {
    TempWorkspace workspace;
    ToolHost host;

    agent::tools::PatchRequest request;
    request.patch_text = "this is not a patch";

    auto result = host.apply_patch(workspace.root(), request);
    ASSERT_TRUE(is_error(result));
    EXPECT_EQ(get_error(result).code, "invalid_patch_format");
}

}  // namespace
