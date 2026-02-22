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
    EXPECT_NE(tool_result.output.find("matches=3"), std::string::npos);
    EXPECT_NE(tool_result.output.find("a.cpp:1"), std::string::npos);
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

}  // namespace
