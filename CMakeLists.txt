cmake_minimum_required(VERSION 3.20)
project(CodingAgent VERSION 0.1.0 LANGUAGES CXX)

# Tell CMake to generate the map for your IDE's linter
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Require C++20 standard
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Strict warning flags for quality control
set(COMPILER_WARNINGS -Wall -Wextra -Wpedantic -Werror)

#=================================================
#   Dependencies
#=================================================
include(FetchContent)

# Fetch nlohmann/json (v3.11.3 is the current stable release)
FetchContent_Declare(json
    URL https://github.com/nlohmann/json/releases/download/v3.11.3/json.tar.xz
)
FetchContent_MakeAvailable(json)

#=========================================
#    Targets
#=========================================

# Core library target (contains all your layers)
add_library(agent_core STATIC
    src/core/agent_core.cpp
    src/session/run_manager.cpp
)
target_include_directories(agent_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_compile_options(agent_core PRIVATE ${COMPILER_WARNINGS})

# Link the JSON library to our core agent library
target_link_libraries(agent_core PUBLIC nlohmann_json::nlohmann_json)

# CLI Executable (Interface Layer)
add_executable(agent_cli
    src/app/main.cpp
    src/app/cli_parser.cpp
)
target_link_libraries(agent_cli PRIVATE agent_core)
target_compile_options(agent_cli PRIVATE ${COMPILER_WARNINGS})


# ==========================================
# TESTING BASELINE
# ==========================================
include(CTest)
enable_testing()

# Fetch GoogleTest (Using the standard Git method)
FetchContent_Declare(
  googletest
  GIT_REPOSITORY https://github.com/google/googletest.git
  GIT_TAG        v1.14.0
)
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

# Create the test executable
add_executable(agent_tests
    tests/unit/test_errors.cpp
    tests/unit/test_cli_parser.cpp
    tests/unit/test_run_manager.cpp
    src/app/cli_parser.cpp
)

# Link our core library AND the GoogleTest framework
target_link_libraries(agent_tests PRIVATE
    agent_core
    GTest::gtest_main
)
target_compile_options(agent_tests PRIVATE ${COMPILER_WARNINGS})

# Register the test with CTest so we can run it from the command line
include(GoogleTest)
gtest_discover_tests(agent_tests)
