#include "command_process.hpp"

#include <array>
#include <cerrno>
#include <fcntl.h>
#include <stdexcept>
#include <unistd.h>
#include <sys/wait.h>

namespace frametap::worker::command_process
{
namespace
{
struct Result
{
    int status = -1;
    std::string output;
};

std::vector<char*> argv_for(const std::vector<std::string>& command)
{
    std::vector<char*> result;
    result.reserve(command.size() + 1);
    for (const auto& item : command)
    {
        result.push_back(const_cast<char*>(item.c_str()));
    }
    result.push_back(nullptr);
    return result;
}

Result run(const std::vector<std::string>& command)
{
    if (command.empty())
    {
        throw std::runtime_error("empty external command");
    }
    int pipe_fds[2]{};
    if (::pipe2(pipe_fds, O_CLOEXEC) < 0)
    {
        throw std::runtime_error("could not create command pipe");
    }
    const pid_t child = ::fork();
    if (child < 0)
    {
        ::close(pipe_fds[0]);
        ::close(pipe_fds[1]);
        throw std::runtime_error("could not launch " + command.front());
    }
    if (child == 0)
    {
        ::dup2(pipe_fds[1], STDOUT_FILENO);
        ::close(pipe_fds[0]);
        ::close(pipe_fds[1]);
        auto argv = argv_for(command);
        ::execvp(argv[0], argv.data());
        _exit(127);
    }
    ::close(pipe_fds[1]);
    Result result;
    std::array<char, 512> buffer{};
    for (;;)
    {
        const auto count = ::read(pipe_fds[0], buffer.data(), buffer.size());
        if (count > 0)
        {
            result.output.append(buffer.data(), static_cast<std::size_t>(count));
        } else if (count == 0)
        {
            break;
        } else if (errno != EINTR)
        {
            break;
        }
    }
    ::close(pipe_fds[0]);
    while (::waitpid(child, &result.status, 0) < 0 && errno == EINTR)
    {}
    return result;
}

std::string trim(std::string value)
{
    const auto begin = value.find_first_not_of(" \t\r\n");
    if (begin == std::string::npos)
    {
        return {};
    }
    const auto end = value.find_last_not_of(" \t\r\n");
    return value.substr(begin, end - begin + 1);
}
}

std::string checked_output(const std::vector<std::string>& command)
{
    const auto result = run(command);
    if (!WIFEXITED(result.status) || WEXITSTATUS(result.status) != 0)
    {
        throw std::runtime_error(command.front() + " command failed");
    }
    return trim(result.output);
}

void checked(const std::vector<std::string>& command)
{
    static_cast<void>(checked_output(command));
}

pid_t spawn(const std::vector<std::string>& command)
{
    if (command.empty())
    {
        throw std::runtime_error("empty external command");
    }
    const pid_t child = ::fork();
    if (child < 0)
    {
        throw std::runtime_error("could not launch " + command.front());
    }
    if (child == 0)
    {
        ::setpgid(0, 0);
        const int null_fd = ::open("/dev/null", O_RDONLY | O_CLOEXEC);
        if (null_fd >= 0)
        {
            ::dup2(null_fd, STDIN_FILENO);
            ::close(null_fd);
        }
        auto argv = argv_for(command);
        ::execvp(argv[0], argv.data());
        _exit(127);
    }
    ::setpgid(child, child);
    return child;
}
}
