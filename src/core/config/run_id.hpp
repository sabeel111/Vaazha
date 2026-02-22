#pragma once
#include <string>
#include <random>
#include <sstream>

namespace agent::core::config {

    // Generates a simple 8-character hex ID prefixed with "run-"
    inline std::string generate_run_id() {
        std::random_device rd;
        std::mt19937 gen(rd()); // Standard mersenne_twister_engine
        std::uniform_int_distribution<> dis(0, 15);

        std::stringstream ss;
        ss << "run-";
        for (int i = 0; i < 8; ++i) {
            ss << std::hex << dis(gen);
        }
        return ss.str();
    }

} // namespace agent::core::config
