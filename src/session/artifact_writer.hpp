#pragma once

#include <filesystem>
#include <optional>
#include <string>
#include "core/errors/agent_errors.hpp"
#include "protocol/run_execution_contract.hpp"
#include "protocol/run_request.hpp"

namespace agent::session {

class ArtifactWriter {
public:
    explicit ArtifactWriter(std::filesystem::path workspace_root,
                            std::filesystem::path artifact_subdir = ".agent_runs");

    core::errors::Result<std::filesystem::path> write_request(
        const std::string& run_id, const protocol::RunRequest& request) const;

    core::errors::Result<std::filesystem::path> write_step(
        const std::string& run_id, const protocol::RunStep& step) const;

    core::errors::Result<std::filesystem::path> write_final(
        const std::string& run_id, protocol::RunStatus status,
        const std::string& summary,
        const std::optional<std::string>& error_message = std::nullopt) const;

    core::errors::Result<std::filesystem::path> run_log_path(
        const std::string& run_id) const;

private:
    core::errors::Result<std::filesystem::path> append_event(
        const std::string& run_id, const std::string& event_json) const;

    std::filesystem::path workspace_root_;
    std::filesystem::path artifact_subdir_;
};

}  // namespace agent::session
