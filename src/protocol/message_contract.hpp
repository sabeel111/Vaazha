#pragma once
#include <string>
#include <vector>
#include <optional>
#include "tool_contract.hpp" // We include this so the compiler knows what a ToolCall is

namespace agent::protocol {

    enum class Role {
        User,
        Assistant,
        System,
        Tool
    };

    // The upgraded internal message format supporting parallel execution.
    struct Message {
        Role role;
        std::string content;

        // 1. FOR THE ASSISTANT:
        // If the LLM decides to use tools, it populates this list.
        // Using a vector means it can request 1 or 10 tools at the exact same time.
        std::vector<ToolCall> tool_calls;

        // 2. FOR THE TOOL RESULT:
        // When your Execution Layer sends a message back (Role::Tool),
        // it puts the ID of the specific tool it just ran right here.
        std::optional<std::string> tool_call_id;
    };

} // namespace agent::protocol
