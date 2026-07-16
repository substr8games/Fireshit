#pragma once

#include <chrono>
#include <cstdint>
#include <functional>
#include <string>
#include <utility>
#include <vector>
#include <sys/types.h>
#include "frametap/protocol/packet.hpp"

namespace frametap::worker
{
class LaunchedApplication
{
  public:
    LaunchedApplication(const std::vector<std::string>& command,
      const std::vector<std::pair<std::string, std::string>>& environment = {});
    ~LaunchedApplication();
    LaunchedApplication(const LaunchedApplication&) = delete;
    LaunchedApplication& operator=(const LaunchedApplication&) = delete;

    protocol::ViewRecord wait_for_view(
      const std::vector<protocol::ViewRecord>& baseline,
      const std::function<std::vector<protocol::ViewRecord>()>& catalog,
      std::chrono::seconds timeout);
    bool alive() noexcept;
    void interrupt() noexcept;
    int finish() noexcept;
    pid_t pid() const noexcept;

  private:
    bool belongs_to_launch(std::uint32_t candidate) const noexcept;
    bool was_present(std::uint64_t view_id,
      const std::vector<protocol::ViewRecord>& baseline) const noexcept;
    void reap_nonblocking() noexcept;

    pid_t child = -1;
    pid_t process_group = -1;
    bool reaped = false;
    int status = 0;
};
}
