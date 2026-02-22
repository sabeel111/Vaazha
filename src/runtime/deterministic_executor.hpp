#pragma once

#include <atomic>
#include <memory>
#include <string>
#include "core/errors/agent_errors.hpp"
#include "protocol/run_execution_contract.hpp"
#include "protocol/run_request.hpp"

namespace agent::runtime {

class DeterministicExecutor {
public:
    core::errors::Result<protocol::RunResult> execute(
        const std::string& run_id, const protocol::RunRequest& request,
        std::shared_ptr<std::atomic_bool> cancel_token = nullptr) const;
};

}  // namespace agent::runtime
