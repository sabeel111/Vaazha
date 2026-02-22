#pragma once
#include <string>

namespace agent::protocol {

    // How the LLM asks your Execution Layer to do something
    struct ToolCall {
        std::string id;
        std::string name;       // e.g., "read_file", "run_command"
        std::string arguments;  // Raw JSON string of the arguments
    };

    // How your Execution Layer replies back
    struct ToolResult {
        std::string tool_call_id;
        bool success;
        std::string output;         // stdout or file content
        std::string error_message;  // stderr or failure reason
        double duration_ms;         // useful for observability
    };

} // namespace agent::protocol
