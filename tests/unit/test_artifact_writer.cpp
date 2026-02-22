#include <filesystem>
#include <fstream>
#include <string>
#include <vector>
#include <gtest/gtest.h>
#include <nlohmann/json.hpp>
#include "core/config/run_id.hpp"
#include "core/errors/agent_errors.hpp"
#include "protocol/run_execution_contract.hpp"
#include "protocol/run_request.hpp"
#include "session/artifact_writer.hpp"

namespace {

using agent::core::errors::get_error;
using agent::core::errors::get_value;
using agent::core::errors::is_error;
using agent::protocol::RunRequest;
using agent::protocol::RunStatus;
using agent::protocol::RunStep;
using agent::protocol::RunStepType;
using agent::session::ArtifactWriter;
using nlohmann::json;

class TempWorkspace {
public:
    TempWorkspace() {
        root_ = std::filesystem::current_path() /
                (".tmp_artifact_writer_" + agent::core::config::generate_run_id());
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

std::vector<std::string> read_lines(const std::filesystem::path& file_path) {
    std::vector<std::string> lines;
    std::ifstream in(file_path);
    std::string line;
    while (std::getline(in, line)) {
        lines.push_back(line);
    }
    return lines;
}

RunRequest make_request(const std::filesystem::path& workspace) {
    RunRequest request;
    request.task_description = "Write artifact events";
    request.working_directory = workspace;
    request.max_steps = 5;
    request.verbose = true;
    return request;
}

TEST(ArtifactWriterTest, WritesRequestStepAndFinalEvents) {
    TempWorkspace workspace;
    ArtifactWriter writer(workspace.root());
    const std::string run_id = "run-artifacts-1";

    const auto request_result = writer.write_request(run_id, make_request(workspace.root()));
    ASSERT_FALSE(is_error(request_result));
    const auto log_path = get_value(request_result);
    EXPECT_TRUE(std::filesystem::exists(log_path));

    RunStep step;
    step.id = "step-1";
    step.type = RunStepType::InspectRequest;
    step.success = true;
    step.output = "checked request";
    const auto step_result = writer.write_step(run_id, step);
    ASSERT_FALSE(is_error(step_result));

    const auto final_result =
        writer.write_final(run_id, RunStatus::Completed, "all good");
    ASSERT_FALSE(is_error(final_result));

    const auto lines = read_lines(log_path);
    ASSERT_EQ(lines.size(), 3u);

    const auto request_event = json::parse(lines[0]);
    EXPECT_EQ(request_event.at("event").get<std::string>(), "request");
    EXPECT_EQ(request_event.at("run_id").get<std::string>(), run_id);
    EXPECT_EQ(request_event.at("payload").at("task_description").get<std::string>(),
              "Write artifact events");

    const auto step_event = json::parse(lines[1]);
    EXPECT_EQ(step_event.at("event").get<std::string>(), "step");
    EXPECT_EQ(step_event.at("payload").at("id").get<std::string>(), "step-1");
    EXPECT_EQ(step_event.at("payload").at("type").get<std::string>(),
              "inspect_request");

    const auto final_event = json::parse(lines[2]);
    EXPECT_EQ(final_event.at("event").get<std::string>(), "final");
    EXPECT_EQ(final_event.at("payload").at("status").get<std::string>(), "completed");
    EXPECT_EQ(final_event.at("payload").at("summary").get<std::string>(), "all good");
}

TEST(ArtifactWriterTest, FailsForInvalidWorkspaceRoot) {
    const auto missing_root =
        std::filesystem::current_path() /
        ("__missing_artifact_root__" + agent::core::config::generate_run_id());
    std::error_code ec;
    std::filesystem::remove_all(missing_root, ec);

    ArtifactWriter writer(missing_root);
    auto result = writer.write_request("run-artifacts-2", make_request(missing_root));
    ASSERT_TRUE(is_error(result));
    EXPECT_EQ(get_error(result).code, "invalid_workspace_root");
}

}  // namespace
