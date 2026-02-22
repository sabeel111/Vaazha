#pragma once

#include <filesystem>
#include <string>
#include <vector>
#include "core/errors/agent_errors.hpp"

namespace agent::policy {

struct CommandPolicy {
    std::vector<std::string> blocked_substrings = {
        "sudo",
        "rm -rf",
        "shutdown",
        "reboot",
        "mkfs",
        "dd if=",
        ":(){ :|:& };:"};
};

class PolicyGuard {
public:
    explicit PolicyGuard(CommandPolicy command_policy = {});

    core::errors::Result<std::filesystem::path> validate_path_in_workspace(
        const std::filesystem::path& workspace_root,
        const std::filesystem::path& target_path) const;

    core::errors::Result<std::string> validate_command(
        const std::string& command) const;

private:
    static bool is_within_root(const std::filesystem::path& root,
                               const std::filesystem::path& child);
    static std::string lowercase(std::string value);

    CommandPolicy command_policy_;
};

}  // namespace agent::policy
