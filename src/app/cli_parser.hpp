#pragma once
#include "protocol/run_request.hpp"
#include "core/errors/agent_errors.hpp"

namespace agent::app::cli {
    agent::core::errors::Result<agent::protocol::RunRequest> parse_and_validate(int argc, char* argv[]);
}
