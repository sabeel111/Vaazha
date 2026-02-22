#include "cli_parser.hpp"
#include <vector>
#include <charconv>

namespace agent::app::cli {

    using namespace agent::core::errors;
    using agent::protocol::RunRequest;

    // 1. Raw Options Struct (Internal only)
    struct RawCliOptions {
        std::optional<std::string> task;
        std::optional<std::string> plan_file;
        std::optional<std::string> cwd;
        std::optional<std::string> max_steps;
        bool verbose = false;
    };

    Result<RunRequest> parse_and_validate(int argc, char* argv[]) {
        if (argc < 2) {
            return AgentError{ErrorCategory::Input, "No command provided.", "missing_command", "Usage: agent_cli run --task \"...\""};
        }

        std::string command = argv[1];
        if (command != "run") {
            return AgentError{ErrorCategory::Input, "Unknown command: " + command, "unknown_command", "Currently only the 'run' command is supported."};
        }

        RawCliOptions raw;
        std::vector<std::string> args;
        for (int i = 2; i < argc; ++i) { // Start at 2 to skip program name and 'run' command
            args.push_back(argv[i]);
        }

        // 2. Parser Phase: Just read the raw strings
        for (size_t i = 0; i < args.size(); ++i) {
            if (args[i] == "--task") {
                if (i + 1 < args.size()) raw.task = args[++i];
                else return AgentError{ErrorCategory::Input, "Missing value for --task", "missing_value"};
            } else if (args[i] == "--plan-file") {
                if (i + 1 < args.size()) raw.plan_file = args[++i];
                else return AgentError{ErrorCategory::Input, "Missing value for --plan-file", "missing_value"};
            } else if (args[i] == "--cwd") {
                if (i + 1 < args.size()) raw.cwd = args[++i];
                else return AgentError{ErrorCategory::Input, "Missing value for --cwd", "missing_value"};
            } else if (args[i] == "--max-steps") {
                if (i + 1 < args.size()) raw.max_steps = args[++i];
                else return AgentError{ErrorCategory::Input, "Missing value for --max-steps", "missing_value"};
            } else if (args[i] == "--verbose") {
                raw.verbose = true;
            } else {
                return AgentError{ErrorCategory::Input, "Unknown argument: " + args[i], "unknown_argument"};
            }
        }

        // 3. Validator Phase: Enforce logic and bounds
        RunRequest req;
        req.verbose = raw.verbose;

        // Mutual Exclusion XOR check
        if (!raw.task.has_value() && !raw.plan_file.has_value()) {
            return AgentError{ErrorCategory::Input, "Must provide either --task or --plan-file", "missing_required_flag"};
        }
        if (raw.task.has_value() && raw.plan_file.has_value()) {
            return AgentError{ErrorCategory::Input, "Cannot provide both --task and --plan-file", "conflicting_flags"};
        }

        if (raw.task) req.task_description = raw.task.value();
        if (raw.plan_file) req.plan_file = std::filesystem::path(raw.plan_file.value());

        // Exception-free integer parsing
        if (raw.max_steps) {
            uint32_t steps = 0;
            auto [ptr, ec] = std::from_chars(raw.max_steps->data(), raw.max_steps->data() + raw.max_steps->size(), steps);
            if (ec != std::errc()) {
                return AgentError{ErrorCategory::Input, "Invalid number for --max-steps", "invalid_integer", "Provide a positive integer."};
            }
            if (steps == 0 || steps > 1000) {
                return AgentError{ErrorCategory::Input, "--max-steps out of bounds", "bounds_error", "Must be between 1 and 1000."};
            }
            req.max_steps = steps;
        }

        // Path validation
        if (raw.cwd) {
            std::filesystem::path p(raw.cwd.value());
            if (!std::filesystem::exists(p) || !std::filesystem::is_directory(p)) {
                return AgentError{ErrorCategory::Input, "Working directory does not exist or is not a directory", "invalid_path"};
            }
            req.working_directory = std::filesystem::canonical(p);
        }

        return req;
    }

} // namespace agent::app::cli
