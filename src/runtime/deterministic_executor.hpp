#pragma once

#include <string>
#include "core/errors/agent_errors.hpp"
#include "protocol/run_execution_contract.hpp"
#include "protocol/run_request.hpp"

namespace agent::runtime {

class DeterministicExecutor {
public:
    core::errors::Result<protocol::RunResult> execute(
        const std::string& run_id, const protocol::RunRequest& request) const;
};

}  // namespace agent::runtime
