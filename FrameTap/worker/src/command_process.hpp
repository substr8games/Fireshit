#pragma once

#include <string>
#include <vector>
#include <sys/types.h>

namespace frametap::worker::command_process
{
std::string checked_output(const std::vector<std::string>& command);
void checked(const std::vector<std::string>& command);
pid_t spawn(const std::vector<std::string>& command);
}
