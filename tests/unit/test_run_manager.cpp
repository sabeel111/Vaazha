#include <filesystem>
#include <string>
#include <gtest/gtest.h>
#include "core/errors/agent_errors.hpp"
#include "protocol/run_request.hpp"
#include "session/run_manager.hpp"

namespace {

using agent::core::errors::get_error;
using agent::core::errors::get_value;
using agent::core::errors::is_error;
using agent::protocol::RunRequest;
using agent::session::RunManager;
using agent::session::RunState;

RunRequest make_valid_request() {
    RunRequest req;
    req.task_description = "Implement a deterministic run manager";
    req.working_directory = std::filesystem::current_path();
    req.max_steps = 10;
    req.verbose = false;
    return req;
}

TEST(RunManagerTest, StartRunMovesToRunning) {
    RunManager manager;
    auto start = manager.start_run(make_valid_request());
    ASSERT_FALSE(is_error(start));

    const std::string run_id = get_value(start);
    auto state = manager.get_run_state(run_id);
    ASSERT_FALSE(is_error(state));
    EXPECT_EQ(get_value(state), RunState::Running);
}

TEST(RunManagerTest, CancelRunMovesToCancelled) {
    RunManager manager;
    auto start = manager.start_run(make_valid_request());
    ASSERT_FALSE(is_error(start));

    const std::string run_id = get_value(start);
    auto cancel = manager.cancel_run(run_id);
    ASSERT_FALSE(is_error(cancel));
    EXPECT_EQ(get_value(cancel), RunState::Cancelled);

    auto state = manager.get_run_state(run_id);
    ASSERT_FALSE(is_error(state));
    EXPECT_EQ(get_value(state), RunState::Cancelled);
}

TEST(RunManagerTest, CancelRunSetsCancellationToken) {
    RunManager manager;
    auto start = manager.start_run(make_valid_request());
    ASSERT_FALSE(is_error(start));
    const std::string run_id = get_value(start);

    auto token_result = manager.get_cancel_token(run_id);
    ASSERT_FALSE(is_error(token_result));
    auto token = get_value(token_result);
    ASSERT_TRUE(token != nullptr);
    EXPECT_FALSE(token->load());

    auto cancel = manager.cancel_run(run_id);
    ASSERT_FALSE(is_error(cancel));
    EXPECT_TRUE(token->load());
}

TEST(RunManagerTest, CancelRunFailsForUnknownId) {
    RunManager manager;
    auto cancel = manager.cancel_run("run-does-not-exist");
    ASSERT_TRUE(is_error(cancel));
    EXPECT_EQ(get_error(cancel).code, "run_not_found");
}

TEST(RunManagerTest, GetCancelTokenFailsForUnknownRun) {
    RunManager manager;
    auto token = manager.get_cancel_token("run-does-not-exist");
    ASSERT_TRUE(is_error(token));
    EXPECT_EQ(get_error(token).code, "run_not_found");
}

TEST(RunManagerTest, CancelRunFailsAfterCompletion) {
    RunManager manager;
    auto start = manager.start_run(make_valid_request());
    ASSERT_FALSE(is_error(start));

    const std::string run_id = get_value(start);
    auto complete = manager.mark_completed(run_id);
    ASSERT_FALSE(is_error(complete));
    EXPECT_EQ(get_value(complete), RunState::Completed);

    auto cancel = manager.cancel_run(run_id);
    ASSERT_TRUE(is_error(cancel));
    EXPECT_EQ(get_error(cancel).code, "invalid_state_transition");
}

TEST(RunManagerTest, StartRunRejectsEmptyRequest) {
    RunManager manager;
    RunRequest req;
    req.task_description.reset();
    req.plan_file.reset();

    auto start = manager.start_run(req);
    ASSERT_TRUE(is_error(start));
    EXPECT_EQ(get_error(start).code, "invalid_run_request");
}

TEST(RunManagerTest, StartRunRejectsConflictingRequest) {
    RunManager manager;
    RunRequest req = make_valid_request();
    req.plan_file = std::filesystem::path("plan.json");

    auto start = manager.start_run(req);
    ASSERT_TRUE(is_error(start));
    EXPECT_EQ(get_error(start).code, "invalid_run_request");
}

}  // namespace
