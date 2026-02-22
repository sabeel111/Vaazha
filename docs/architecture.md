Architecture Decision Records (ADR)
Project: Coding Agent (C++ Port)
Last Updated: Phase 0 Completion

1. Error Handling Strategy
Decision: Result-Object Pattern (std::variant) over C++ Exceptions.
Context: The agent execution loop must be highly deterministic. If a tool (like a bash command) fails, it is usually not a fatal system crash; it is just feedback that the LLM needs to see to correct its mistake.
Implications:

C++ try/catch blocks are strictly reserved for unrecoverable fatal errors (e.g., out of memory).

All internal functions that can fail must return Result<T> (a std::variant holding either the success payload or an AgentError).

The compiler forces the caller to handle the error state, eliminating silent crashes.

2. The Canonical Internal Protocol
Decision: Strict C++ Structs (Message, ToolCall, ToolResult) for all internal communication.
Context: Inspired by the pi-mono architecture, the strongest pattern for an agent is a stable internal protocol combined with adapter edges.
Implications:

The core Agent Loop knows nothing about OpenAI, Anthropic, or Gemini APIs.

Future API adapters will be forced to translate their specific JSON responses into our C++ Message struct before the data reaches the loop.

We specifically included std::vector<ToolCall> to natively support parallel tool execution in the future.

3. Dependency Management
Decision: CMake FetchContent via Git Repository Tags.
Context: We needed a way to pull in third-party libraries (like nlohmann/json and GoogleTest) while guaranteeing that builds are 100% reproducible on any clean machine.
Implications:

Developers do not need to globally install packages via apt or brew to compile the agent.

Dependencies are strictly pinned to specific Git tags (e.g., v1.14.0) to prevent upstream updates from breaking the build.

4. Serialization Format
Decision: JSON (nlohmann/json).
Context: We evaluated protobuf and flatbuffers, but LLMs natively consume and produce JSON. Furthermore, our Phase 1 Session Storage requires append-only JSONL files.
Implications:

JSON is the standard data interchange format for the agent.

The nlohmann/json library is heavily utilized at the boundaries (API adapters and Artifact logging) to parse strings into our Canonical C++ Protocol.

5. Build Quality Standards
Decision: C++20 with Strict Compiler Warnings (-Werror, -Wall, -Wextra, -Wpedantic).
Context: To prevent memory leaks, unhandled edge cases, and undefined behavior early in the development cycle.
Implications:

The codebase uses modern C++20 features (std::optional, std::variant).

A single unused variable or missing return type will intentionally fail the build.

Code style is strictly enforced via .clang-format.
