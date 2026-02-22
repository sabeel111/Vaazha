#pragma once

#include <string>
#include <vector>

namespace agent::protocol {

enum class RunStatus {
    Completed,
    Failed
};

enum class RunStepType {
    InspectRequest,
    LoadContext,
    ExecuteCommand,
    ApplyPatch,
    BuildReport
};

struct RunStep {
    std::string id;
    RunStepType type;
    bool success = false;
    std::string output;
};

struct RunResult {
    std::string run_id;
    RunStatus status = RunStatus::Completed;
    std::vector<RunStep> steps;
    std::string summary;
};

inline std::string to_string(const RunStatus status) {
    switch (status) {
        case RunStatus::Completed:
            return "completed";
        case RunStatus::Failed:
            return "failed";
        default:
            return "unknown";
    }
}

inline std::string to_string(const RunStepType type) {
    switch (type) {
        case RunStepType::InspectRequest:
            return "inspect_request";
        case RunStepType::LoadContext:
            return "load_context";
        case RunStepType::ExecuteCommand:
            return "execute_command";
        case RunStepType::ApplyPatch:
            return "apply_patch";
        case RunStepType::BuildReport:
            return "build_report";
        default:
            return "unknown";
    }
}

}  // namespace agent::protocol
