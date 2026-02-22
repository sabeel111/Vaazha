#pragma once
#include <string>
#include <filesystem>
#include <cstdint>
#include <optional>

namespace agent::protocol {

    // Represents the validated user input required to start an agent loop
    struct RunRequest {
        std::optional<std::string> task_description;
        std::optional<std::filesystem::path> plan_file; // Future-proofing
        std::filesystem::path working_directory = std::filesystem::current_path();
        uint32_t max_steps = 30;
        bool verbose = false;
    };

} // namespace agent::protocol
