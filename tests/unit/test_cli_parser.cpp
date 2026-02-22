#include <filesystem>
#include <string>
#include <vector>
#include <gtest/gtest.h>
#include "app/cli_parser.hpp"
#include "core/errors/agent_errors.hpp"

namespace {

using agent::app::cli::parse_and_validate;
using agent::core::errors::ErrorCategory;
using agent::core::errors::get_error;
using agent::core::errors::get_value;
using agent::core::errors::is_error;
using agent::protocol::RunRequest;

agent::core::errors::Result<RunRequest> parse_tokens(
    const std::vector<std::string>& tokens) {
    std::vector<std::string> owned_args;
    owned_args.reserve(tokens.size() + 1);
    owned_args.emplace_back("agent_cli");
    for (const auto& token : tokens) {
        owned_args.push_back(token);
    }

    std::vector<char*> argv;
    argv.reserve(owned_args.size());
    for (auto& arg : owned_args) {
        argv.push_back(arg.data());
    }

    return parse_and_validate(static_cast<int>(argv.size()), argv.data());
}

TEST(CliParserTest, FailsWhenCommandMissing) {
    auto result = parse_tokens({});
    ASSERT_TRUE(is_error(result));
    EXPECT_EQ(get_error(result).category, ErrorCategory::Input);
    EXPECT_EQ(get_error(result).code, "missing_command");
}

TEST(CliParserTest, FailsWhenCommandUnknown) {
    auto result = parse_tokens({"status"});
    ASSERT_TRUE(is_error(result));
    EXPECT_EQ(get_error(result).code, "unknown_command");
}

TEST(CliParserTest, FailsWhenRequiredTaskOrPlanMissing) {
    auto result = parse_tokens({"run"});
    ASSERT_TRUE(is_error(result));
    EXPECT_EQ(get_error(result).code, "missing_required_flag");
}

TEST(CliParserTest, FailsWhenTaskAndPlanBothProvided) {
    auto result = parse_tokens({"run", "--task", "fix issue", "--plan-file", "plan.md"});
    ASSERT_TRUE(is_error(result));
    EXPECT_EQ(get_error(result).code, "conflicting_flags");
}

TEST(CliParserTest, FailsWhenMaxStepsNotNumeric) {
    auto result = parse_tokens({"run", "--task", "fix issue", "--max-steps", "abc"});
    ASSERT_TRUE(is_error(result));
    EXPECT_EQ(get_error(result).code, "invalid_integer");
}

TEST(CliParserTest, FailsWhenMaxStepsHasTrailingCharacters) {
    auto result = parse_tokens({"run", "--task", "fix issue", "--max-steps", "12abc"});
    ASSERT_TRUE(is_error(result));
    EXPECT_EQ(get_error(result).code, "invalid_integer");
}

TEST(CliParserTest, FailsWhenMaxStepsOutOfBounds) {
    auto result = parse_tokens({"run", "--task", "fix issue", "--max-steps", "0"});
    ASSERT_TRUE(is_error(result));
    EXPECT_EQ(get_error(result).code, "bounds_error");
}

TEST(CliParserTest, FailsWhenCwdInvalid) {
    const auto missing_dir =
        std::filesystem::current_path() / "__definitely_missing_cli_parser_test_dir__";
    auto result = parse_tokens(
        {"run", "--task", "fix issue", "--cwd", missing_dir.string()});
    ASSERT_TRUE(is_error(result));
    EXPECT_EQ(get_error(result).code, "invalid_path");
}

TEST(CliParserTest, ParsesValidTaskRequest) {
    const auto cwd = std::filesystem::current_path();
    auto result = parse_tokens({"run", "--task", "fix issue", "--cwd", cwd.string(),
                                "--max-steps", "42", "--verbose"});
    ASSERT_FALSE(is_error(result));

    const auto& req = get_value(result);
    ASSERT_TRUE(req.task_description.has_value());
    EXPECT_EQ(req.task_description.value(), "fix issue");
    EXPECT_FALSE(req.plan_file.has_value());
    EXPECT_EQ(req.max_steps, 42u);
    EXPECT_TRUE(req.verbose);
    EXPECT_TRUE(std::filesystem::exists(req.working_directory));
}

TEST(CliParserTest, ParsesValidPlanFileRequest) {
    auto result = parse_tokens({"run", "--plan-file", "plans/step1.json"});
    ASSERT_FALSE(is_error(result));

    const auto& req = get_value(result);
    EXPECT_FALSE(req.task_description.has_value());
    ASSERT_TRUE(req.plan_file.has_value());
    EXPECT_EQ(req.plan_file->string(), "plans/step1.json");
    EXPECT_EQ(req.max_steps, 30u);
    EXPECT_FALSE(req.verbose);
}

}  // namespace
