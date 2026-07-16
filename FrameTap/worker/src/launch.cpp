#include "frametap/worker/launch.hpp"

#include <cerrno>
#include <csignal>
#include <cstring>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <thread>
#include <unistd.h>
#include <sys/wait.h>

namespace frametap::worker
{
namespace
{
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

pid_t parent_pid(pid_t pid) noexcept
{
    std::ifstream stat("/proc/" + std::to_string(pid) + "/stat");
    std::string line;
    if (!std::getline(stat, line))
    {
        return -1;
    }

    const auto comm_end = line.rfind(')');
    if (comm_end == std::string::npos || comm_end + 2 >= line.size())
    {
        return -1;
    }

    std::istringstream fields(line.substr(comm_end + 2));
    char state = 0;
    pid_t parent = -1;
    fields >> state >> parent;
    return fields ? parent : -1;
}
}

LaunchedApplication::LaunchedApplication(const std::vector<std::string>& command,
  const std::vector<std::pair<std::string, std::string>>& environment)
{
    if (command.empty())
    {
        throw std::runtime_error("launch command is empty");
    }

    child = ::fork();
    if (child < 0)
    {
        throw std::runtime_error(std::string("could not launch application: ") +
          std::strerror(errno));
    }

    if (child == 0)
    {
        ::setpgid(0, 0);
        for (const auto& [name, value] : environment)
        {
            ::setenv(name.c_str(), value.c_str(), 1);
        }
        auto argv = argv_for(command);
        ::execvp(argv[0], argv.data());
        _exit(127);
    }

    process_group = child;
    ::setpgid(child, process_group);
}

LaunchedApplication::~LaunchedApplication()
{
    reap_nonblocking();
}

protocol::ViewRecord LaunchedApplication::wait_for_view(
  const std::vector<protocol::ViewRecord>& baseline,
  const std::function<std::vector<protocol::ViewRecord>()>& catalog,
  std::chrono::seconds timeout)
{
    const auto deadline = std::chrono::steady_clock::now() + timeout;
    while (std::chrono::steady_clock::now() < deadline)
    {
        for (const auto& view : catalog())
        {
            if (!view.mapped || was_present(view.view_id, baseline))
            {
                continue;
            }

            if (belongs_to_launch(view.pid))
            {
                return view;
            }
        }

        reap_nonblocking();
        if (reaped && !alive())
        {
            throw std::runtime_error("launched application exited before opening a Wayland window");
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }

    throw std::runtime_error("timed out waiting for the launched application window");
}

bool LaunchedApplication::alive() noexcept
{
    reap_nonblocking();
    if (process_group <= 0)
    {
        return false;
    }

    if (::kill(-process_group, 0) == 0 || errno == EPERM)
    {
        return true;
    }

    return false;
}

void LaunchedApplication::interrupt() noexcept
{
    if (process_group > 0)
    {
        ::kill(-process_group, SIGINT);
    }
}

int LaunchedApplication::finish() noexcept
{
    if (!reaped && child > 0)
    {
        while (::waitpid(child, &status, 0) < 0 && errno == EINTR)
        {}
        reaped = true;
    }

    if (WIFEXITED(status))
    {
        return WEXITSTATUS(status);
    }

    if (WIFSIGNALED(status))
    {
        return 128 + WTERMSIG(status);
    }

    return 0;
}

pid_t LaunchedApplication::pid() const noexcept
{
    return child;
}

bool LaunchedApplication::belongs_to_launch(std::uint32_t candidate) const noexcept
{
    if (candidate == 0 || child <= 0)
    {
        return false;
    }

    pid_t current = static_cast<pid_t>(candidate);
    for (int depth = 0; depth < 64 && current > 1; ++depth)
    {
        if (current == child ||
          (process_group > 0 && ::getpgid(current) == process_group))
        {
            return true;
        }

        const auto parent = parent_pid(current);
        if (parent <= 0 || parent == current)
        {
            break;
        }
        current = parent;
    }

    return false;
}

bool LaunchedApplication::was_present(std::uint64_t view_id,
  const std::vector<protocol::ViewRecord>& baseline) const noexcept
{
    for (const auto& view : baseline)
    {
        if (view.view_id == view_id)
        {
            return true;
        }
    }

    return false;
}

void LaunchedApplication::reap_nonblocking() noexcept
{
    if (reaped || child <= 0)
    {
        return;
    }

    const auto result = ::waitpid(child, &status, WNOHANG);
    if (result == child)
    {
        reaped = true;
    }
}
}
