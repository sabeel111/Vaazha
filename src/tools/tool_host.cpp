#include "tools/tool_host.hpp"

#include <chrono>
#include <fstream>
#include <sstream>
#include <system_error>
#include <vector>
#include "policy/policy_guard.hpp"

namespace agent::tools {

using protocol::ToolResult;

namespace {

bool is_probably_binary(const std::filesystem::path& path) {
    std::ifstream in(path, std::ios::binary);
    if (!in.is_open()) {
        return false;
    }

    constexpr std::size_t kProbeSize = 1024;
    char buffer[kProbeSize];
    in.read(buffer, static_cast<std::streamsize>(kProbeSize));
    const std::streamsize read_bytes = in.gcount();
    for (std::streamsize i = 0; i < read_bytes; ++i) {
        if (buffer[i] == '\0') {
            return true;
        }
    }
    return false;
}

std::string trim_line(const std::string& line) {
    constexpr std::size_t kMaxLineLength = 240;
    if (line.size() <= kMaxLineLength) {
        return line;
    }
    return line.substr(0, kMaxLineLength) + "...";
}

}  // namespace

core::errors::Result<ToolResult> ToolHost::read_file(
    const std::filesystem::path& workspace_root,
    const std::filesystem::path& path) const {
    const auto started = std::chrono::steady_clock::now();
    const policy::PolicyGuard policy_guard;
    auto resolved = policy_guard.validate_path_in_workspace(workspace_root, path);
    if (core::errors::is_error(resolved)) {
        return core::errors::get_error(resolved);
    }
    const std::filesystem::path file_path = core::errors::get_value(resolved);

    std::error_code ec;
    if (!std::filesystem::exists(file_path, ec) || ec) {
        return ToolResult{"read_file", false, "", "File does not exist: " + file_path.string(),
                          0.0};
    }
    if (!std::filesystem::is_regular_file(file_path, ec) || ec) {
        return ToolResult{"read_file", false, "",
                          "Path is not a regular file: " + file_path.string(), 0.0};
    }
    if (is_probably_binary(file_path)) {
        return ToolResult{"read_file", false, "",
                          "Refusing to read binary file: " + file_path.string(), 0.0};
    }

    std::ifstream in(file_path);
    if (!in.is_open()) {
        return ToolResult{"read_file", false, "",
                          "Failed to open file: " + file_path.string(), 0.0};
    }

    std::ostringstream buffer;
    buffer << in.rdbuf();
    if (!in.good() && !in.eof()) {
        return ToolResult{"read_file", false, "",
                          "I/O error while reading file: " + file_path.string(), 0.0};
    }

    const auto ended = std::chrono::steady_clock::now();
    const double elapsed_ms =
        std::chrono::duration<double, std::milli>(ended - started).count();

    return ToolResult{"read_file", true, buffer.str(), "", elapsed_ms};
}

core::errors::Result<ToolResult> ToolHost::search(
    const std::filesystem::path& workspace_root,
    const SearchRequest& request) const {
    if (request.pattern.empty()) {
        return core::errors::AgentError{
            core::errors::ErrorCategory::Input,
            "Search pattern cannot be empty.",
            "empty_search_pattern"};
    }
    if (request.max_matches == 0) {
        return core::errors::AgentError{
            core::errors::ErrorCategory::Input,
            "max_matches must be greater than zero.",
            "invalid_search_limit"};
    }

    const auto started = std::chrono::steady_clock::now();
    const policy::PolicyGuard policy_guard;
    auto resolved =
        policy_guard.validate_path_in_workspace(workspace_root, request.scope);
    if (core::errors::is_error(resolved)) {
        return core::errors::get_error(resolved);
    }
    const std::filesystem::path scope_path = core::errors::get_value(resolved);

    std::error_code ec;
    if (!std::filesystem::exists(scope_path, ec) || ec) {
        return ToolResult{"search", false, "", "Scope does not exist: " + scope_path.string(),
                          0.0};
    }

    std::vector<std::filesystem::path> files;
    if (std::filesystem::is_regular_file(scope_path, ec) && !ec) {
        files.push_back(scope_path);
    } else if (std::filesystem::is_directory(scope_path, ec) && !ec) {
        const auto options = std::filesystem::directory_options::skip_permission_denied;
        for (const auto& entry :
             std::filesystem::recursive_directory_iterator(scope_path, options, ec)) {
            if (ec) {
                continue;
            }
            if (!entry.is_regular_file(ec) || ec) {
                continue;
            }
            files.push_back(entry.path());
        }
    } else {
        return ToolResult{"search", false, "",
                          "Scope is neither a file nor directory: " + scope_path.string(),
                          0.0};
    }

    constexpr std::uintmax_t kMaxFileBytes = 1024 * 1024;
    std::ostringstream out;
    std::size_t matches = 0;
    for (const auto& file : files) {
        if (matches >= request.max_matches) {
            break;
        }

        const auto size = std::filesystem::file_size(file, ec);
        if (ec || size > kMaxFileBytes) {
            continue;
        }
        if (is_probably_binary(file)) {
            continue;
        }

        std::ifstream in(file);
        if (!in.is_open()) {
            continue;
        }

        std::string line;
        std::size_t line_no = 0;
        while (std::getline(in, line)) {
            ++line_no;
            if (line.find(request.pattern) == std::string::npos) {
                continue;
            }
            out << file.string() << ":" << line_no << ":" << trim_line(line) << "\n";
            ++matches;
            if (matches >= request.max_matches) {
                break;
            }
        }
    }

    const auto ended = std::chrono::steady_clock::now();
    const double elapsed_ms =
        std::chrono::duration<double, std::milli>(ended - started).count();

    std::ostringstream header;
    header << "pattern=\"" << request.pattern << "\" scope=\""
           << scope_path.string() << "\" matches=" << matches << "\n";
    if (matches == 0) {
        header << "No matches found.";
    } else {
        header << out.str();
    }

    return ToolResult{"search", true, header.str(), "", elapsed_ms};
}

}  // namespace agent::tools
