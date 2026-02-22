#include <iostream>
#include <string>
#include "app/cli_parser.hpp"
#include "core/config/run_id.hpp"
#include "core/errors/agent_errors.hpp"
#include "core/logging/logger.hpp"

int main(int argc, char* argv[]) {
    // 1. Generate a unique Run ID for this execution
    std::string run_id = agent::core::config::generate_run_id();

    // 2. Register the Run ID with the Global Logger
    agent::core::logging::Logger::get().set_run_id(run_id);

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
    LOG_INFO("CLI request validated.");
    if (req.task_description.has_value()) {
        LOG_INFO("Task: " + req.task_description.value());
    }
    if (req.plan_file.has_value()) {
        LOG_INFO("Plan file: " + req.plan_file.value().string());
    }
    LOG_INFO("Working directory: " + req.working_directory.string());
    LOG_INFO("Max steps: " + std::to_string(req.max_steps));
    LOG_INFO(std::string("Verbose: ") + (req.verbose ? "true" : "false"));

    return 0;
}
