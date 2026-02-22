#pragma once

#include <cstddef>
#include <filesystem>
#include <string>
#include "core/errors/agent_errors.hpp"
#include "protocol/tool_contract.hpp"

namespace agent::tools {

struct SearchRequest {
    std::string pattern;
    std::filesystem::path scope = ".";
    std::size_t max_matches = 20;
};

class ToolHost {
public:
    core::errors::Result<protocol::ToolResult> read_file(
        const std::filesystem::path& workspace_root,
        const std::filesystem::path& path) const;

    core::errors::Result<protocol::ToolResult> search(
        const std::filesystem::path& workspace_root,
        const SearchRequest& request) const;

private:
    static core::errors::Result<std::filesystem::path> resolve_within_root(
        const std::filesystem::path& workspace_root,
        const std::filesystem::path& target);
};

}  // namespace agent::tools
