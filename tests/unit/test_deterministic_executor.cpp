#include <filesystem>
#include <fstream>
#include <string>
#include <gtest/gtest.h>
#include "core/errors/agent_errors.hpp"
#include "protocol/run_request.hpp"
#include "runtime/deterministic_executor.hpp"

namespace {

using agent::core::errors::get_error;
using agent::core::errors::get_value;
using agent::core::errors::is_error;
using agent::protocol::RunRequest;
using agent::protocol::RunStatus;
using agent::runtime::DeterministicExecutor;

RunRequest make_task_request() {
    RunRequest req;
    req.task_description = "Refactor parser";
    req.working_directory = std::filesystem::current_path();
    return req;
}

TEST(DeterministicExecutorTest, ExecutesTaskRequest) {
    DeterministicExecutor executor;
    auto result = executor.execute("run-test-1", make_task_request());
    ASSERT_FALSE(is_error(result));

    const auto& run_result = get_value(result);
    EXPECT_EQ(run_result.run_id, "run-test-1");
    EXPECT_EQ(run_result.status, RunStatus::Completed);
    EXPECT_EQ(run_result.steps.size(), 3u);
    EXPECT_FALSE(run_result.summary.empty());
}

TEST(DeterministicExecutorTest, ExecutesPlanFileRequest) {
    const auto temp_file = std::filesystem::current_path() / "test-plan-step4.txt";
    {
        std::ofstream out(temp_file);
        out << "step: demo";
    }

    RunRequest req;
    req.plan_file = temp_file.filename();
    req.working_directory = std::filesystem::current_path();

    DeterministicExecutor executor;
    auto result = executor.execute("run-test-2", req);
    ASSERT_FALSE(is_error(result));

    const auto& run_result = get_value(result);
    EXPECT_EQ(run_result.steps.size(), 3u);
    EXPECT_NE(run_result.steps[1].output.find("Loaded plan file"), std::string::npos);

    std::error_code ec;
    std::filesystem::remove(temp_file, ec);
}

TEST(DeterministicExecutorTest, FailsWhenPlanFileMissing) {
    RunRequest req;
    req.plan_file = "missing-step4-plan.txt";
    req.working_directory = std::filesystem::current_path();

    DeterministicExecutor executor;
    auto result = executor.execute("run-test-3", req);
    ASSERT_TRUE(is_error(result));
    EXPECT_EQ(get_error(result).code, "plan_file_read_failed");
}

}  // namespace
