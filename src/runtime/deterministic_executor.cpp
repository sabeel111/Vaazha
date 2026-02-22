#include "runtime/deterministic_executor.hpp"

#include <atomic>
#include <cctype>
#include <memory>
#include "tools/tool_host.hpp"

namespace agent::runtime {

using core::errors::AgentError;
using core::errors::ErrorCategory;
using protocol::RunRequest;
using protocol::RunResult;
using protocol::RunStatus;
using protocol::RunStep;
using protocol::RunStepType;
using tools::CommandRequest;
using tools::PatchRequest;
using tools::SearchRequest;
using tools::ToolHost;

namespace {

std::string pick_search_pattern(const std::string& task) {
    std::string token;
    std::string fallback;
    for (const char c : task) {
        if (std::isalnum(static_cast<unsigned char>(c)) != 0) {
            token.push_back(c);
            continue;
        }
        if (!token.empty()) {
            if (fallback.empty()) {
                fallback = token;
            }
            if (token.size() >= 4) {
                return token;
            }
            token.clear();
        }
    }
    if (!token.empty()) {
        if (fallback.empty()) {
            fallback = token;
        }
        if (token.size() >= 4) {
            return token;
        }
    }
    if (!fallback.empty()) {
        return fallback;
    }
    return "TODO";
}

bool looks_like_patch(const std::string& text) {
    return text.find("+++ ") != std::string::npos &&
           text.find("--- ") != std::string::npos;
}

}  // namespace

core::errors::Result<RunResult> DeterministicExecutor::execute(
    const std::string& run_id, const RunRequest& request,
    std::shared_ptr<std::atomic_bool> cancel_token) const {
    RunResult result;
    result.run_id = run_id;
    result.status = RunStatus::Completed;

    int next_step_id = 1;
    auto make_step_id = [&next_step_id]() {
        return std::string("step-") + std::to_string(next_step_id++);
    };

    RunStep inspect_step;
    inspect_step.id = make_step_id();
    inspect_step.type = RunStepType::InspectRequest;
    inspect_step.success = true;
    inspect_step.output = request.plan_file.has_value() ? "mode=plan_file" : "mode=task";
    result.steps.push_back(inspect_step);

    ToolHost tool_host;
    std::string context_payload;
    std::string plan_contents;

    RunStep context_step;
    context_step.id = make_step_id();
    context_step.type = RunStepType::LoadContext;
    context_step.success = true;

    if (request.plan_file.has_value()) {
        auto read_result =
            tool_host.read_file(request.working_directory, request.plan_file.value());
        if (core::errors::is_error(read_result)) {
            return core::errors::get_error(read_result);
        }
        const auto& tool_result = core::errors::get_value(read_result);
        if (!tool_result.success) {
            return AgentError{ErrorCategory::Execution,
                              "Failed to read plan file: " + tool_result.error_message,
                              "plan_file_read_failed"};
        }
        plan_contents = tool_result.output;
        context_payload = "Loaded plan file (" +
                          std::to_string(tool_result.output.size()) + " bytes)";
    } else if (request.task_description.has_value()) {
        SearchRequest search_request;
        search_request.pattern = pick_search_pattern(request.task_description.value());
        search_request.scope = ".";
        search_request.max_matches = 12;

        auto search_result = tool_host.search(request.working_directory, search_request);
        if (core::errors::is_error(search_result)) {
            return core::errors::get_error(search_result);
        }
        const auto& tool_result = core::errors::get_value(search_result);
        if (!tool_result.success) {
            return AgentError{ErrorCategory::Execution,
                              "Search failed: " + tool_result.error_message,
                              "search_failed"};
        }

        context_payload = "Task: " + request.task_description.value() + "\n" +
                          tool_result.output;
    } else {
        return AgentError{ErrorCategory::Input,
                          "Request has neither task nor plan file.",
                          "invalid_run_request"};
    }

    context_step.output = context_payload;
    result.steps.push_back(context_step);

    RunStep command_step;
    command_step.id = make_step_id();
    command_step.type = RunStepType::ExecuteCommand;
    command_step.success = true;

    CommandRequest command_request;
    command_request.command = "echo command_runner_ok";
    command_request.working_directory = ".";
    command_request.timeout_ms = 2000;
    command_request.cancel_token = cancel_token;

    auto command_result = tool_host.run_command(request.working_directory, command_request);
    if (core::errors::is_error(command_result)) {
        return core::errors::get_error(command_result);
    }
    const auto& command_tool_result = core::errors::get_value(command_result);
    if (!command_tool_result.success) {
        return AgentError{ErrorCategory::Execution,
                          "Command step failed: " +
                              command_tool_result.error_message,
                          "command_failed"};
    }
    command_step.output = command_tool_result.output;
    result.steps.push_back(command_step);

    if (!plan_contents.empty() && looks_like_patch(plan_contents)) {
        RunStep patch_step;
        patch_step.id = make_step_id();
        patch_step.type = RunStepType::ApplyPatch;
        patch_step.success = true;

        PatchRequest patch_request;
        patch_request.patch_text = plan_contents;
        patch_request.timeout_ms = 4000;
        patch_request.cancel_token = cancel_token;

        auto patch_result = tool_host.apply_patch(request.working_directory, patch_request);
        if (core::errors::is_error(patch_result)) {
            return core::errors::get_error(patch_result);
        }
        const auto& patch_tool_result = core::errors::get_value(patch_result);
        if (!patch_tool_result.success) {
            return AgentError{ErrorCategory::Execution,
                              "Patch step failed: " +
                                  patch_tool_result.error_message,
                              "apply_patch_failed"};
        }
        patch_step.output = "Patch applied successfully.";
        result.steps.push_back(patch_step);
    }

    RunStep report_step;
    report_step.id = make_step_id();
    report_step.type = RunStepType::BuildReport;
    report_step.success = true;
    report_step.output = "Prepared deterministic report context";
    result.steps.push_back(report_step);

    result.summary = "Deterministic execution completed with " +
                     std::to_string(result.steps.size()) + " steps.";
    return result;
}

}  // namespace agent::runtime
