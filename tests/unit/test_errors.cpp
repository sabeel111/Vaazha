#include <gtest/gtest.h>
#include "core/errors/agent_errors.hpp"

using namespace agent::core::errors;

// A dummy function to simulate a tool failing
Result<std::string> simulate_read_file(bool should_fail) {
    if (should_fail) {
        return AgentError{ErrorCategory::Execution, "File not found"};
    }
    return std::string("file contents here");
}

TEST(ErrorModelTest, HandlesSuccess) {
    auto result = simulate_read_file(false);

    // Check that it is NOT an error
    EXPECT_FALSE(is_error(result));
    // Check that the value is correct
    EXPECT_EQ(get_value(result), "file contents here");
}

TEST(ErrorModelTest, HandlesFailure) {
    auto result = simulate_read_file(true);

    // Check that it IS an error
    EXPECT_TRUE(is_error(result));

    // Check the error details
    auto error = get_error(result);
    EXPECT_EQ(error.category, ErrorCategory::Execution);
    EXPECT_EQ(error.message, "File not found");
}
