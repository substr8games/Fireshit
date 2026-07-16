#pragma once

#include <filesystem>
#include <vector>
#include <wayland-server-core.h>
#include "frametap/plugin/capture_backend.hpp"
#include "frametap/protocol/packet.hpp"

namespace frametap::plugin
{
class Server
{
  public:
    void start(wl_event_loop* loop);
    void stop() noexcept;
  private:
    static int on_accept(int fd, std::uint32_t mask, void* data);
    static int on_client(int fd, std::uint32_t mask, void* data);
    int accept_client();
    int handle_client(std::uint32_t mask);
    void close_client() noexcept;
    std::vector<protocol::ViewRecord> catalog_views() const;
    std::vector<protocol::OutputRecord> catalog_outputs() const;
    wayfire_view resolve_target(const protocol::PrepareSession& request) const;
    void reply(protocol::Packet packet);

    std::filesystem::path socket_path;
    int listen_fd = -1;
    int client_fd = -1;
    wl_event_source* listen_source = nullptr;
    wl_event_source* client_source = nullptr;
    CaptureBackend capture;
    std::uint64_t next_session_id = 1;
};
}
