#include "runtime/deterministic_executor.hpp"

#include <filesystem>
#include <fstream>
#include <sstream>

namespace agent::runtime {

using core::errors::AgentError;
using core::errors::ErrorCategory;
using protocol::RunRequest;
using protocol::RunResult;
using protocol::RunStatus;
using protocol::RunStep;
using protocol::RunStepType;

namespace {

std::string read_file_contents(const std::filesystem::path& path,
                               std::error_code& ec) {
    std::ifstream stream(path);
    if (!stream.is_open()) {
        ec = std::make_error_code(std::errc::no_such_file_or_directory);
        return {};
    }

    std::ostringstream buffer;
    buffer << stream.rdbuf();
    if (!stream.good() && !stream.eof()) {
        ec = std::make_error_code(std::errc::io_error);
        return {};
    }

    ec.clear();
    return buffer.str();
}

}  // namespace

core::errors::Result<RunResult> DeterministicExecutor::execute(
    const std::string& run_id, const RunRequest& request) const {
    RunResult result;
    result.run_id = run_id;
    result.status = RunStatus::Completed;

    RunStep inspect_step;
    inspect_step.id = "step-1";
    inspect_step.type = RunStepType::InspectRequest;
    inspect_step.success = true;
    inspect_step.output = request.plan_file.has_value() ? "mode=plan_file" : "mode=task";
    result.steps.push_back(inspect_step);

    RunStep context_step;
    context_step.id = "step-2";
    context_step.type = RunStepType::LoadContext;
    context_step.success = true;

    std::string context_payload;
    if (request.plan_file.has_value()) {
        std::filesystem::path plan_path = request.plan_file.value();
        if (plan_path.is_relative()) {
            plan_path = request.working_directory / plan_path;
        }

        std::error_code ec;
        plan_path = std::filesystem::weakly_canonical(plan_path, ec);
        if (ec) {
            return AgentError{ErrorCategory::Execution,
                              "Unable to resolve plan file path: " + plan_path.string(),
                              "plan_file_resolve_failed"};
        }

        std::string file_contents = read_file_contents(plan_path, ec);
        if (ec) {
            return AgentError{ErrorCategory::Execution,
                              "Failed to read plan file: " + plan_path.string(),
                              "plan_file_read_failed"};
        }

        context_payload = "Loaded plan file (" + std::to_string(file_contents.size()) +
                          " bytes) from " + plan_path.string();
    } else if (request.task_description.has_value()) {
        context_payload = "Task: " + request.task_description.value();
    } else {
        return AgentError{ErrorCategory::Input,
                          "Request has neither task nor plan file.",
                          "invalid_run_request"};
    }

    context_step.output = context_payload;
    result.steps.push_back(context_step);

    RunStep report_step;
    report_step.id = "step-3";
    report_step.type = RunStepType::BuildReport;
    report_step.success = true;
    report_step.output = "Prepared deterministic report context";
    result.steps.push_back(report_step);

    result.summary = "Deterministic execution completed with " +
                     std::to_string(result.steps.size()) + " steps.";
    return result;
}

}  // namespace agent::runtime
