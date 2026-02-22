#include <iostream>
#include <string>
#include "app/cli_parser.hpp"
#include "core/config/run_id.hpp"
#include "core/errors/agent_errors.hpp"
#include "core/logging/logger.hpp"
#include "protocol/run_execution_contract.hpp"
#include "runtime/deterministic_executor.hpp"
#include "session/run_manager.hpp"

int main(int argc, char* argv[]) {
    // 1. Generate a unique Run ID for this execution
    std::string bootstrap_run_id = agent::core::config::generate_run_id();

    // 2. Register the Run ID with the Global Logger
    agent::core::logging::Logger::get().set_run_id(bootstrap_run_id);

    // 3. Parse CLI input and return normalized input errors
    LOG_INFO("Agent Interface Layer: Bootstrapping...");
    auto parsed = agent::app::cli::parse_and_validate(argc, argv);
    if (agent::core::errors::is_error(parsed)) {
        const auto& err = agent::core::errors::get_error(parsed);
        LOG_ERROR("Input error [" + err.code + "]: " + err.message);
        if (!err.hint.empty()) {
            LOG_INFO("Hint: " + err.hint);
        }
        return 2;
    }

    const auto& req = agent::core::errors::get_value(parsed);
    agent::session::RunManager run_manager;
    auto started = run_manager.start_run(req);
    if (agent::core::errors::is_error(started)) {
        const auto& err = agent::core::errors::get_error(started);
        LOG_ERROR("Failed to start run [" + err.code + "]: " + err.message);
        return 3;
    }

    const std::string run_id = agent::core::errors::get_value(started);
    agent::core::logging::Logger::get().set_run_id(run_id);
    LOG_INFO("Run started: " + run_id);

    agent::runtime::DeterministicExecutor executor;
    auto execution = executor.execute(run_id, req);
    if (agent::core::errors::is_error(execution)) {
        const auto& err = agent::core::errors::get_error(execution);
        LOG_ERROR("Execution failed [" + err.code + "]: " + err.message);
        auto failed = run_manager.mark_failed(run_id, err.message);
        if (agent::core::errors::is_error(failed)) {
            const auto& fail_err = agent::core::errors::get_error(failed);
            LOG_ERROR("Failed to mark run as failed [" + fail_err.code + "]: " +
                      fail_err.message);
        }
        return 1;
    }

    const auto& result = agent::core::errors::get_value(execution);
    for (const auto& step : result.steps) {
        LOG_INFO("Step " + step.id + " (" + agent::protocol::to_string(step.type) +
                 "): " + (step.success ? "ok" : "failed"));
        LOG_INFO("  output: " + step.output);
    }
    LOG_INFO("Run summary: " + result.summary);

    auto completed = run_manager.mark_completed(run_id);
    if (agent::core::errors::is_error(completed)) {
        const auto& err = agent::core::errors::get_error(completed);
        LOG_ERROR("Failed to mark run as completed [" + err.code + "]: " +
                  err.message);
        return 4;
    }

    const auto current_state = run_manager.get_run_state(run_id);
    if (agent::core::errors::is_error(current_state)) {
        const auto& err = agent::core::errors::get_error(current_state);
        LOG_ERROR("Failed to fetch final run state [" + err.code + "]: " +
                  err.message);
        return 5;
    }

    const auto state = agent::core::errors::get_value(current_state);
    std::string state_text = "unknown";
    switch (state) {
        case agent::session::RunState::Created:
            state_text = "created";
            break;
        case agent::session::RunState::Running:
            state_text = "running";
            break;
        case agent::session::RunState::Completed:
            state_text = "completed";
            break;
        case agent::session::RunState::Failed:
            state_text = "failed";
            break;
        case agent::session::RunState::Cancelled:
            state_text = "cancelled";
            break;
    }

    LOG_INFO("Final run state: " + state_text);

    return 0;
}
