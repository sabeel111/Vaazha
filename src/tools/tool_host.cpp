#include "tools/tool_host.hpp"

#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <ctime>
#include <cerrno>
#include <fcntl.h>
#include <fstream>
#include <poll.h>
#include <signal.h>
#include <sstream>
#include <string>
#include <sys/wait.h>
#include <system_error>
#include <unistd.h>
#include <unordered_set>
#include <vector>
#include "policy/policy_guard.hpp"

namespace agent::tools {

using protocol::ToolResult;

namespace {

struct ProcessCapture {
    int exit_code = -1;
    bool timed_out = false;
    bool cancelled = false;
    std::string stdout_text;
    std::string stderr_text;
    double duration_ms = 0.0;
};

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

void set_nonblocking(const int fd) {
    const int flags = fcntl(fd, F_GETFL, 0);
    if (flags == -1) {
        return;
    }
    static_cast<void>(fcntl(fd, F_SETFL, flags | O_NONBLOCK));
}

void drain_pipe(const int fd, bool& is_open, std::string& out) {
    if (!is_open) {
        return;
    }

    char buffer[4096];
    while (true) {
        const ssize_t n = read(fd, buffer, sizeof(buffer));
        if (n > 0) {
            out.append(buffer, static_cast<std::size_t>(n));
            continue;
        }
        if (n == 0) {
            is_open = false;
            static_cast<void>(close(fd));
            return;
        }
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            return;
        }
        is_open = false;
        static_cast<void>(close(fd));
        return;
    }
}

std::string shell_escape_single_quotes(const std::string& value) {
    std::string escaped;
    escaped.reserve(value.size() + 16);
    for (const char c : value) {
        if (c == '\'') {
            escaped += "'\\''";
        } else {
            escaped.push_back(c);
        }
    }
    return escaped;
}

core::errors::Result<ProcessCapture> run_shell_command(
    const std::string& command, const std::filesystem::path& cwd,
    const std::uint32_t timeout_ms,
    const std::shared_ptr<std::atomic_bool>& cancel_token) {
    if (cancel_token && cancel_token->load()) {
        ProcessCapture capture;
        capture.cancelled = true;
        capture.stderr_text = "Command cancelled before start.";
        return capture;
    }

    int stdout_pipe[2] = {-1, -1};
    int stderr_pipe[2] = {-1, -1};
    if (pipe(stdout_pipe) != 0 || pipe(stderr_pipe) != 0) {
        return core::errors::AgentError{
            core::errors::ErrorCategory::Internal,
            "Failed to create process pipes.",
            "pipe_creation_failed"};
    }

    const auto started = std::chrono::steady_clock::now();
    const pid_t pid = fork();
    if (pid < 0) {
        static_cast<void>(close(stdout_pipe[0]));
        static_cast<void>(close(stdout_pipe[1]));
        static_cast<void>(close(stderr_pipe[0]));
        static_cast<void>(close(stderr_pipe[1]));
        return core::errors::AgentError{core::errors::ErrorCategory::Internal,
                                        "Failed to fork process.",
                                        "fork_failed"};
    }

    if (pid == 0) {
        if (chdir(cwd.c_str()) != 0) {
            _exit(126);
        }
        static_cast<void>(dup2(stdout_pipe[1], STDOUT_FILENO));
        static_cast<void>(dup2(stderr_pipe[1], STDERR_FILENO));
        static_cast<void>(close(stdout_pipe[0]));
        static_cast<void>(close(stdout_pipe[1]));
        static_cast<void>(close(stderr_pipe[0]));
        static_cast<void>(close(stderr_pipe[1]));
        execl("/bin/sh", "sh", "-lc", command.c_str(), static_cast<char*>(nullptr));
        _exit(127);
    }

    static_cast<void>(close(stdout_pipe[1]));
    static_cast<void>(close(stderr_pipe[1]));
    set_nonblocking(stdout_pipe[0]);
    set_nonblocking(stderr_pipe[0]);

    ProcessCapture capture;
    bool stdout_open = true;
    bool stderr_open = true;
    bool child_exited = false;
    int status = 0;

    while (stdout_open || stderr_open || !child_exited) {
        if (cancel_token && cancel_token->load() && !child_exited) {
            capture.cancelled = true;
            static_cast<void>(kill(pid, SIGKILL));
        }

        const auto now = std::chrono::steady_clock::now();
        const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                                 now - started)
                                 .count();
        if (!capture.timed_out && timeout_ms > 0 &&
            elapsed > static_cast<std::int64_t>(timeout_ms) && !child_exited) {
            capture.timed_out = true;
            static_cast<void>(kill(pid, SIGKILL));
        }

        pollfd fds[2];
        nfds_t nfds = 0;
        if (stdout_open) {
            fds[nfds].fd = stdout_pipe[0];
            fds[nfds].events = POLLIN;
            ++nfds;
        }
        if (stderr_open) {
            fds[nfds].fd = stderr_pipe[0];
            fds[nfds].events = POLLIN;
            ++nfds;
        }

        if (nfds > 0) {
            static_cast<void>(poll(fds, nfds, 50));
        }

        drain_pipe(stdout_pipe[0], stdout_open, capture.stdout_text);
        drain_pipe(stderr_pipe[0], stderr_open, capture.stderr_text);

        if (!child_exited) {
            const pid_t waited = waitpid(pid, &status, WNOHANG);
            if (waited == pid) {
                child_exited = true;
            }
        }

        if (child_exited && !stdout_open && !stderr_open) {
            break;
        }
    }

    if (!child_exited) {
        static_cast<void>(waitpid(pid, &status, 0));
    }

    if (WIFEXITED(status)) {
        capture.exit_code = WEXITSTATUS(status);
    } else if (WIFSIGNALED(status)) {
        capture.exit_code = 128 + WTERMSIG(status);
    } else {
        capture.exit_code = -1;
    }

    const auto ended = std::chrono::steady_clock::now();
    capture.duration_ms =
        std::chrono::duration<double, std::milli>(ended - started).count();
    return capture;
}

std::vector<std::filesystem::path> extract_patch_paths(
    const std::string& patch_text) {
    std::istringstream in(patch_text);
    std::string line;
    std::unordered_set<std::string> dedupe;
    std::vector<std::filesystem::path> paths;

    while (std::getline(in, line)) {
        if (!(line.rfind("+++ ", 0) == 0 || line.rfind("--- ", 0) == 0)) {
            continue;
        }
        std::string candidate = line.substr(4);
        if (candidate == "/dev/null") {
            continue;
        }

        const auto tab_pos = candidate.find('\t');
        if (tab_pos != std::string::npos) {
            candidate = candidate.substr(0, tab_pos);
        }

        if (candidate.rfind("a/", 0) == 0 || candidate.rfind("b/", 0) == 0) {
            candidate = candidate.substr(2);
        }

        if (candidate.empty()) {
            continue;
        }
        if (dedupe.insert(candidate).second) {
            paths.emplace_back(candidate);
        }
    }
    return paths;
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

core::errors::Result<ToolResult> ToolHost::run_command(
    const std::filesystem::path& workspace_root,
    const CommandRequest& request) const {
    const policy::PolicyGuard policy_guard;
    auto validated_command = policy_guard.validate_command(request.command);
    if (core::errors::is_error(validated_command)) {
        return core::errors::get_error(validated_command);
    }

    auto validated_cwd = policy_guard.validate_path_in_workspace(
        workspace_root, request.working_directory);
    if (core::errors::is_error(validated_cwd)) {
        return core::errors::get_error(validated_cwd);
    }

    auto capture_result = run_shell_command(
        core::errors::get_value(validated_command),
        core::errors::get_value(validated_cwd), request.timeout_ms,
        request.cancel_token);
    if (core::errors::is_error(capture_result)) {
        return core::errors::get_error(capture_result);
    }
    const auto capture = core::errors::get_value(capture_result);

    ToolResult result;
    result.tool_call_id = "run_command";
    result.output = capture.stdout_text;
    result.error_message = capture.stderr_text;
    result.duration_ms = capture.duration_ms;

    if (capture.cancelled) {
        result.success = false;
        if (!result.error_message.empty()) {
            result.error_message += "\n";
        }
        result.error_message += "Command cancelled.";
        return result;
    }

    if (capture.timed_out) {
        result.success = false;
        if (!result.error_message.empty()) {
            result.error_message += "\n";
        }
        result.error_message += "Command timed out.";
        return result;
    }

    result.success = (capture.exit_code == 0);
    if (!result.success && result.error_message.empty()) {
        result.error_message =
            "Command failed with exit code " + std::to_string(capture.exit_code);
    }
    return result;
}

core::errors::Result<ToolResult> ToolHost::apply_patch(
    const std::filesystem::path& workspace_root,
    const PatchRequest& request) const {
    if (request.patch_text.empty()) {
        return core::errors::AgentError{core::errors::ErrorCategory::Input,
                                        "Patch text cannot be empty.",
                                        "empty_patch"};
    }

    const auto patch_paths = extract_patch_paths(request.patch_text);
    if (patch_paths.empty()) {
        return core::errors::AgentError{
            core::errors::ErrorCategory::Input,
            "Patch does not include any file paths.",
            "invalid_patch_format"};
    }

    const policy::PolicyGuard policy_guard;
    for (const auto& patch_path : patch_paths) {
        auto validated = policy_guard.validate_path_in_workspace(workspace_root, patch_path);
        if (core::errors::is_error(validated)) {
            return core::errors::get_error(validated);
        }
    }

    std::error_code ec;
    const auto artifacts_dir = workspace_root / ".agent_runs";
    std::filesystem::create_directories(artifacts_dir, ec);
    if (ec) {
        return core::errors::AgentError{
            core::errors::ErrorCategory::Internal,
            "Failed to create temporary patch directory: " + artifacts_dir.string(),
            "patch_temp_dir_failed"};
    }

    const auto patch_file = artifacts_dir /
                            ("tool_patch_" + std::to_string(std::time(nullptr)) + "_" +
                             std::to_string(std::rand()) + ".diff");
    {
        std::ofstream out(patch_file);
        if (!out.is_open()) {
            return core::errors::AgentError{
                core::errors::ErrorCategory::Internal,
                "Failed to open temporary patch file: " + patch_file.string(),
                "patch_temp_open_failed"};
        }
        out << request.patch_text;
        if (!out.good()) {
            return core::errors::AgentError{
                core::errors::ErrorCategory::Internal,
                "Failed to write temporary patch file: " + patch_file.string(),
                "patch_temp_write_failed"};
        }
    }

    CommandRequest command_request;
    command_request.command =
        "patch -p1 --forward --batch -i '" +
        shell_escape_single_quotes(patch_file.string()) + "'";
    command_request.working_directory = ".";
    command_request.timeout_ms = request.timeout_ms;
    command_request.cancel_token = request.cancel_token;

    auto command_result = run_command(workspace_root, command_request);

    std::filesystem::remove(patch_file, ec);
    if (core::errors::is_error(command_result)) {
        return core::errors::get_error(command_result);
    }

    auto tool_result = core::errors::get_value(command_result);
    tool_result.tool_call_id = "apply_patch";
    return tool_result;
}

}  // namespace agent::tools
