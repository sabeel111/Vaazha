#pragma once

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <string>
#include "core/errors/agent_errors.hpp"
#include "protocol/tool_contract.hpp"

namespace agent::tools {

struct SearchRequest {
    std::string pattern;
    std::filesystem::path scope = ".";
    std::size_t max_matches = 20;
};

struct CommandRequest {
    std::string command;
    std::filesystem::path working_directory = ".";
    std::uint32_t timeout_ms = 5000;
    std::shared_ptr<std::atomic_bool> cancel_token;
};

struct PatchRequest {
    std::string patch_text;
    std::uint32_t timeout_ms = 5000;
    std::shared_ptr<std::atomic_bool> cancel_token;
};

class ToolHost {
public:
    core::errors::Result<protocol::ToolResult> read_file(
        const std::filesystem::path& workspace_root,
        const std::filesystem::path& path) const;

    core::errors::Result<protocol::ToolResult> search(
        const std::filesystem::path& workspace_root,
        const SearchRequest& request) const;

    core::errors::Result<protocol::ToolResult> run_command(
        const std::filesystem::path& workspace_root,
        const CommandRequest& request) const;

    core::errors::Result<protocol::ToolResult> apply_patch(
        const std::filesystem::path& workspace_root,
        const PatchRequest& request) const;
};

}  // namespace agent::tools
