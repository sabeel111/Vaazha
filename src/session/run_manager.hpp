#pragma once

#include <cstddef>
#include <atomic>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <unordered_map>
#include "core/errors/agent_errors.hpp"
#include "protocol/run_request.hpp"

namespace agent::session {

enum class RunState {
    Created,
    Running,
    Completed,
    Failed,
    Cancelled
};

struct RunRecord {
    std::string run_id;
    protocol::RunRequest request;
    RunState state = RunState::Created;
    std::optional<std::string> failure_reason;
    std::shared_ptr<std::atomic_bool> cancel_token;
};

class RunManager {
public:
    core::errors::Result<std::string> start_run(const protocol::RunRequest& request);
    core::errors::Result<RunState> cancel_run(const std::string& run_id);
    core::errors::Result<RunState> get_run_state(const std::string& run_id) const;
    core::errors::Result<std::shared_ptr<std::atomic_bool>> get_cancel_token(
        const std::string& run_id) const;

    // Helpers for deterministic pipeline wiring in upcoming phase tasks.
    core::errors::Result<RunState> mark_completed(const std::string& run_id);
    core::errors::Result<RunState> mark_failed(const std::string& run_id,
                                               const std::string& reason);

    std::size_t run_count() const;

private:
    core::errors::Result<RunState> transition_to_terminal(
        const std::string& run_id, RunState next_state,
        const std::optional<std::string>& failure_reason);
    static bool is_terminal(RunState state);
    static std::string to_string(RunState state);

    mutable std::mutex mutex_;
    std::unordered_map<std::string, RunRecord> runs_;
};

}  // namespace agent::session
