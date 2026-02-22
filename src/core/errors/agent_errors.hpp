#pragma once
#include <string>
#include <variant>

namespace agent::core::errors {

    // 1. Define typed error categories
    enum class ErrorCategory {
        Input,      // E.g., User provided an invalid CLI flag
        Execution,  // E.g., A tool or bash command failed
        Provider,   // E.g., OpenAI/Anthropic API timed out
        Policy,     // E.g., Agent tried to write outside the workspace
        Internal    // E.g., C++ logic bug or parsing failure
    };

    // The standardized error payload
    struct AgentError {
            ErrorCategory category;
            std::string message;
            std::string code = "unknown_error"; // Default value so we don't break old tests
            std::string hint = "";              // Helpful tips for the user
        };

    // 2. Define the Propagation Strategy (Result Object)
    // A Result will hold either a successful value of type T, OR an AgentError.
    template <typename T>
    using Result = std::variant<T, AgentError>;

    // --- Beginner-friendly helpers to work with std::variant ---

    template <typename T>
    bool is_error(const Result<T>& result) {
        return std::holds_alternative<AgentError>(result);
    }

    template <typename T>
    const AgentError& get_error(const Result<T>& result) {
        return std::get<AgentError>(result);
    }

    template <typename T>
    const T& get_value(const Result<T>& result) {
        return std::get<T>(result);
    }

} // namespace agent::core::errors
