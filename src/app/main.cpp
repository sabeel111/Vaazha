#include <iostream>
#include "core/config/run_id.hpp"
#include "core/logging/logger.hpp"

int main() {
    // 1. Generate a unique Run ID for this execution
    std::string run_id = agent::core::config::generate_run_id();

    // 2. Register the Run ID with the Global Logger
    agent::core::logging::Logger::get().set_run_id(run_id);

    // 3. Output our startup logs to satisfy the Phase 0 requirements
    LOG_INFO("Agent Interface Layer: Bootstrapping...");

    // Simulating a config snapshot for now
    LOG_DEBUG("Config snapshot: { environment: 'local', max_threads: 1 }");

    LOG_INFO("Initialization complete. Awaiting commands.");

    return 0;
}
