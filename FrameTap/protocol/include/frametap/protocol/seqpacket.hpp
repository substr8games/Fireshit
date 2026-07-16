#pragma once

#include <filesystem>
#include "frametap/protocol/packet.hpp"

namespace frametap::protocol
{
class UniqueFd
{
  public:
    UniqueFd() = default;
    explicit UniqueFd(int fd) noexcept;
    ~UniqueFd();
    UniqueFd(const UniqueFd&) = delete;
    UniqueFd& operator=(const UniqueFd&) = delete;
    UniqueFd(UniqueFd&& other) noexcept;
    UniqueFd& operator=(UniqueFd&& other) noexcept;
    int get() const noexcept;
    int release() noexcept;
    explicit operator bool() const noexcept;
  private:
    int fd_ = -1;
};

std::filesystem::path default_socket_path();
UniqueFd connect_seqpacket(const std::filesystem::path& path);
void send_packet(int socket_fd, const Packet& packet);
Packet receive_packet(int socket_fd);
}
