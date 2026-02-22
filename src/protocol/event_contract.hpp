#pragma once
#include <string>
#include <variant>

namespace agent::protocol {

    // Enum for why the LLM stopped generating
    enum class StopReason {
        Finished,       // Normal completion
        ToolCall,       // Stopped to run a tool
        MaxTokens,      // Ran out of context window
        Error           // Something broke
    };

    // Define the specific lifecycle events
    struct AgentStartEvent { std::string run_id; };
    struct TurnStartEvent {};
    struct MessageDeltaEvent { std::string delta_text; };
    struct ToolExecutionStartEvent { std::string tool_name; };
    struct ToolExecutionEndEvent { bool success; };
    struct AgentEndEvent { StopReason reason; };

    // std::variant is a C++17/20 feature. It means "AgentEvent" can be
    // exactly ONE of the types listed below. It's perfect for a stream!
    using AgentEvent = std::variant<
        AgentStartEvent,
        TurnStartEvent,
        MessageDeltaEvent,
        ToolExecutionStartEvent,
        ToolExecutionEndEvent,
        AgentEndEvent
    >;

} // namespace agent::protocol
