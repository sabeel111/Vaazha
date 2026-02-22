#include "session/artifact_writer.hpp"

#include <chrono>
#include <fstream>
#include <utility>
#include <nlohmann/json.hpp>

namespace agent::session {

using core::errors::AgentError;
using core::errors::ErrorCategory;
using nlohmann::json;

namespace {

std::int64_t now_unix_ms() {
    const auto now = std::chrono::system_clock::now();
    const auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                        now.time_since_epoch())
                        .count();
    return static_cast<std::int64_t>(ms);
}

json request_to_json(const protocol::RunRequest& request) {
    json payload;
    payload["working_directory"] = request.working_directory.string();
    payload["max_steps"] = request.max_steps;
    payload["verbose"] = request.verbose;
    payload["task_description"] =
        request.task_description.has_value() ? request.task_description.value() : "";
    payload["plan_file"] =
        request.plan_file.has_value() ? request.plan_file.value().string() : "";
    return payload;
}

json step_to_json(const protocol::RunStep& step) {
    json payload;
    payload["id"] = step.id;
    payload["type"] = protocol::to_string(step.type);
    payload["success"] = step.success;
    payload["output"] = step.output;
    return payload;
}

}  // namespace

ArtifactWriter::ArtifactWriter(std::filesystem::path workspace_root,
                               std::filesystem::path artifact_subdir)
    : workspace_root_(std::move(workspace_root)),
      artifact_subdir_(std::move(artifact_subdir)) {}

core::errors::Result<std::filesystem::path> ArtifactWriter::run_log_path(
    const std::string& run_id) const {
    if (run_id.empty()) {
        return AgentError{ErrorCategory::Input, "Run ID cannot be empty.",
                          "invalid_run_id"};
    }

    std::error_code ec;
    if (!std::filesystem::exists(workspace_root_, ec) || ec) {
        return AgentError{ErrorCategory::Input,
                          "Workspace root does not exist: " +
                              workspace_root_.string(),
                          "invalid_workspace_root"};
    }
    if (!std::filesystem::is_directory(workspace_root_, ec) || ec) {
        return AgentError{ErrorCategory::Input,
                          "Workspace root is not a directory: " +
                              workspace_root_.string(),
                          "invalid_workspace_root"};
    }

    const auto canonical_root =
        std::filesystem::weakly_canonical(workspace_root_, ec);
    if (ec) {
        return AgentError{ErrorCategory::Input,
                          "Unable to resolve workspace root: " +
                              workspace_root_.string(),
                          "invalid_workspace_root"};
    }

    auto artifacts_dir = canonical_root / artifact_subdir_;
    std::filesystem::create_directories(artifacts_dir, ec);
    if (ec) {
        return AgentError{ErrorCategory::Internal,
                          "Unable to create artifacts directory: " +
                              artifacts_dir.string(),
                          "artifact_dir_create_failed"};
    }

    const auto run_file = artifacts_dir / (run_id + ".jsonl");
    return run_file;
}

core::errors::Result<std::filesystem::path> ArtifactWriter::append_event(
    const std::string& run_id, const std::string& event_json) const {
    auto run_path_result = run_log_path(run_id);
    if (core::errors::is_error(run_path_result)) {
        return core::errors::get_error(run_path_result);
    }
    const auto run_path = core::errors::get_value(run_path_result);

    std::ofstream out(run_path, std::ios::app);
    if (!out.is_open()) {
        return AgentError{ErrorCategory::Internal,
                          "Unable to open artifact file: " + run_path.string(),
                          "artifact_open_failed"};
    }

    out << event_json << "\n";
    if (!out.good()) {
        return AgentError{ErrorCategory::Internal,
                          "Unable to write artifact event: " + run_path.string(),
                          "artifact_write_failed"};
    }

    return run_path;
}

core::errors::Result<std::filesystem::path> ArtifactWriter::write_request(
    const std::string& run_id, const protocol::RunRequest& request) const {
    json event;
    event["ts_unix_ms"] = now_unix_ms();
    event["event"] = "request";
    event["run_id"] = run_id;
    event["payload"] = request_to_json(request);
    return append_event(run_id, event.dump());
}

core::errors::Result<std::filesystem::path> ArtifactWriter::write_step(
    const std::string& run_id, const protocol::RunStep& step) const {
    json event;
    event["ts_unix_ms"] = now_unix_ms();
    event["event"] = "step";
    event["run_id"] = run_id;
    event["payload"] = step_to_json(step);
    return append_event(run_id, event.dump());
}

core::errors::Result<std::filesystem::path> ArtifactWriter::write_final(
    const std::string& run_id, const protocol::RunStatus status,
    const std::string& summary,
    const std::optional<std::string>& error_message) const {
    json payload;
    payload["status"] = protocol::to_string(status);
    payload["summary"] = summary;
    payload["error_message"] =
        error_message.has_value() ? error_message.value() : "";

    json event;
    event["ts_unix_ms"] = now_unix_ms();
    event["event"] = "final";
    event["run_id"] = run_id;
    event["payload"] = payload;
    return append_event(run_id, event.dump());
}

}  // namespace agent::session
