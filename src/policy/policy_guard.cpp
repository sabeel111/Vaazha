#include "policy/policy_guard.hpp"

#include <algorithm>
#include <cctype>
#include <system_error>
#include <utility>

namespace agent::policy {

using core::errors::AgentError;
using core::errors::ErrorCategory;

PolicyGuard::PolicyGuard(CommandPolicy command_policy)
    : command_policy_(std::move(command_policy)) {}

bool PolicyGuard::is_within_root(const std::filesystem::path& root,
                                 const std::filesystem::path& child) {
    auto root_it = root.begin();
    auto child_it = child.begin();
    for (; root_it != root.end() && child_it != child.end(); ++root_it, ++child_it) {
        if (*root_it != *child_it) {
            return false;
        }
    }
    return root_it == root.end();
}

std::string PolicyGuard::lowercase(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(),
                   [](const unsigned char c) {
                       return static_cast<char>(std::tolower(c));
                   });
    return value;
}

core::errors::Result<std::filesystem::path> PolicyGuard::validate_path_in_workspace(
    const std::filesystem::path& workspace_root,
    const std::filesystem::path& target_path) const {
    std::error_code ec;
    if (!std::filesystem::exists(workspace_root, ec) || ec) {
        return AgentError{ErrorCategory::Input,
                          "Workspace root does not exist: " +
                              workspace_root.string(),
                          "invalid_workspace_root"};
    }
    if (!std::filesystem::is_directory(workspace_root, ec) || ec) {
        return AgentError{ErrorCategory::Input,
                          "Workspace root is not a directory: " +
                              workspace_root.string(),
                          "invalid_workspace_root"};
    }

    const std::filesystem::path canonical_root =
        std::filesystem::weakly_canonical(workspace_root, ec);
    if (ec) {
        return AgentError{ErrorCategory::Input,
                          "Unable to resolve workspace root: " +
                              workspace_root.string(),
                          "invalid_workspace_root"};
    }

    std::filesystem::path candidate = target_path;
    if (candidate.is_relative()) {
        candidate = canonical_root / candidate;
    }

    const std::filesystem::path canonical_candidate =
        std::filesystem::weakly_canonical(candidate, ec);
    if (ec) {
        return AgentError{ErrorCategory::Input,
                          "Unable to resolve target path: " + target_path.string(),
                          "invalid_path"};
    }

    if (!is_within_root(canonical_root, canonical_candidate)) {
        return AgentError{ErrorCategory::Policy,
                          "Path escapes workspace root: " +
                              canonical_candidate.string(),
                          "path_outside_workspace"};
    }

    return canonical_candidate;
}

core::errors::Result<std::string> PolicyGuard::validate_command(
    const std::string& command) const {
    if (command.empty()) {
        return AgentError{ErrorCategory::Input, "Command cannot be empty.",
                          "empty_command"};
    }

    const std::string lowered = lowercase(command);
    for (const auto& blocked : command_policy_.blocked_substrings) {
        const std::string blocked_lowered = lowercase(blocked);
        if (lowered.find(blocked_lowered) == std::string::npos) {
            continue;
        }
        return AgentError{ErrorCategory::Policy,
                          "Command contains blocked operation: " + blocked,
                          "blocked_command"};
    }

    return command;
}

}  // namespace agent::policy
