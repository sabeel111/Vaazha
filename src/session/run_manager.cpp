#include "session/run_manager.hpp"
#include <utility>
#include "core/config/run_id.hpp"
#include "core/logging/logger.hpp"

namespace agent::session {

using core::errors::AgentError;
using core::errors::ErrorCategory;
using protocol::RunRequest;

bool RunManager::is_terminal(const RunState state) {
    return state == RunState::Completed || state == RunState::Failed ||
           state == RunState::Cancelled;
}

std::string RunManager::to_string(const RunState state) {
    switch (state) {
        case RunState::Created:
            return "created";
        case RunState::Running:
            return "running";
        case RunState::Completed:
            return "completed";
        case RunState::Failed:
            return "failed";
        case RunState::Cancelled:
            return "cancelled";
        default:
            return "unknown";
    }
}

core::errors::Result<std::string> RunManager::start_run(
    const RunRequest& request) {
    if (!request.task_description.has_value() &&
        !request.plan_file.has_value()) {
        return AgentError{ErrorCategory::Input,
                          "Run request must include task or plan file.",
                          "invalid_run_request"};
    }
    if (request.task_description.has_value() &&
        request.plan_file.has_value()) {
        return AgentError{ErrorCategory::Input,
                          "Run request cannot include both task and plan file.",
                          "invalid_run_request"};
    }

    std::lock_guard<std::mutex> lock(mutex_);
    constexpr int kMaxAttempts = 16;
    for (int attempt = 0; attempt < kMaxAttempts; ++attempt) {
        const std::string run_id = core::config::generate_run_id();
        if (runs_.find(run_id) != runs_.end()) {
            continue;
        }

        RunRecord record;
        record.run_id = run_id;
        record.request = request;
        record.state = RunState::Created;
        runs_.emplace(run_id, std::move(record));
        LOG_INFO("RunManager: run " + run_id + " transition created -> running");
        runs_[run_id].state = RunState::Running;
        return run_id;
    }

    return AgentError{ErrorCategory::Internal,
                      "Unable to allocate unique run ID.",
                      "run_id_generation_failed"};
}

core::errors::Result<RunState> RunManager::cancel_run(const std::string& run_id) {
    return transition_to_terminal(run_id, RunState::Cancelled, std::nullopt);
}

core::errors::Result<RunState> RunManager::mark_completed(
    const std::string& run_id) {
    return transition_to_terminal(run_id, RunState::Completed, std::nullopt);
}

core::errors::Result<RunState> RunManager::mark_failed(
    const std::string& run_id, const std::string& reason) {
    return transition_to_terminal(run_id, RunState::Failed, reason);
}

core::errors::Result<RunState> RunManager::transition_to_terminal(
    const std::string& run_id, const RunState next_state,
    const std::optional<std::string>& failure_reason) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = runs_.find(run_id);
    if (it == runs_.end()) {
        return AgentError{ErrorCategory::Input, "Run ID not found: " + run_id,
                          "run_not_found"};
    }

    if (is_terminal(it->second.state)) {
        return AgentError{ErrorCategory::Input,
                          "Run is already terminal: " +
                              to_string(it->second.state),
                          "invalid_state_transition"};
    }

    const std::string prev = to_string(it->second.state);
    it->second.state = next_state;
    it->second.failure_reason = failure_reason;
    LOG_INFO("RunManager: run " + run_id + " transition " + prev + " -> " +
             to_string(next_state));
    return it->second.state;
}

core::errors::Result<RunState> RunManager::get_run_state(
    const std::string& run_id) const {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = runs_.find(run_id);
    if (it == runs_.end()) {
        return AgentError{ErrorCategory::Input, "Run ID not found: " + run_id,
                          "run_not_found"};
    }
    return it->second.state;
}

std::size_t RunManager::run_count() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return runs_.size();
}

}  // namespace agent::session
