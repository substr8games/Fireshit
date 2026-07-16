#include "frametap/protocol/seqpacket.hpp"
#include "frametap/protocol/codec.hpp"

#include <array>
#include <cerrno>
#include <cstddef>
#include <cstdlib>
#include <cstring>
#include <stdexcept>
#include <system_error>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>

namespace frametap::protocol
{
UniqueFd::UniqueFd(int fd) noexcept : fd_(fd) {}
UniqueFd::~UniqueFd() { if (fd_ >= 0) ::close(fd_); }
UniqueFd::UniqueFd(UniqueFd&& other) noexcept : fd_(other.release()) {}
UniqueFd& UniqueFd::operator=(UniqueFd&& other) noexcept { if (this != &other) { if (fd_ >= 0) ::close(fd_); fd_ = other.release(); } return *this; }
int UniqueFd::get() const noexcept { return fd_; }
int UniqueFd::release() noexcept { const int value = fd_; fd_ = -1; return value; }
UniqueFd::operator bool() const noexcept { return fd_ >= 0; }

std::filesystem::path default_socket_path()
{
    const char* runtime = std::getenv("XDG_RUNTIME_DIR");
    if (!runtime || !*runtime) throw std::runtime_error("XDG_RUNTIME_DIR is not set");
    return std::filesystem::path(runtime) / "frametap" / "wayfire.sock";
}

UniqueFd connect_seqpacket(const std::filesystem::path& path)
{
    UniqueFd fd(::socket(AF_UNIX, SOCK_SEQPACKET | SOCK_CLOEXEC, 0));
    if (!fd) throw std::system_error(errno, std::generic_category(), "socket");
    sockaddr_un address{}; address.sun_family = AF_UNIX;
    const auto text = path.string();
    if (text.size() >= sizeof(address.sun_path)) throw std::runtime_error("socket path is too long");
    std::memcpy(address.sun_path, text.c_str(), text.size() + 1);
    if (::connect(fd.get(), reinterpret_cast<sockaddr*>(&address), sizeof(address)) < 0)
        throw std::system_error(errno, std::generic_category(), "connect " + text);
    return fd;
}

void send_packet(int socket_fd, const Packet& packet)
{
    const auto bytes = encode_packet(packet);
    iovec iov{const_cast<std::uint8_t*>(bytes.data()), bytes.size()};
    std::array<std::byte, CMSG_SPACE(sizeof(int) * 16)> control{};
    msghdr msg{}; msg.msg_iov = &iov; msg.msg_iovlen = 1;
    if (!packet.fds.empty())
    {
        if (packet.fds.size() > 16) throw std::runtime_error("too many FDs in one scaffold packet");
        msg.msg_control = control.data(); msg.msg_controllen = CMSG_SPACE(sizeof(int) * packet.fds.size());
        auto* cmsg = CMSG_FIRSTHDR(&msg); cmsg->cmsg_level = SOL_SOCKET; cmsg->cmsg_type = SCM_RIGHTS; cmsg->cmsg_len = CMSG_LEN(sizeof(int) * packet.fds.size());
        std::memcpy(CMSG_DATA(cmsg), packet.fds.data(), sizeof(int) * packet.fds.size());
    }
    if (::sendmsg(socket_fd, &msg, MSG_NOSIGNAL) != static_cast<ssize_t>(bytes.size()))
        throw std::system_error(errno, std::generic_category(), "sendmsg");
}

Packet receive_packet(int socket_fd)
{
    std::array<std::uint8_t, max_packet_size> bytes{};
    std::array<std::byte, CMSG_SPACE(sizeof(int) * 16)> control{};
    iovec iov{bytes.data(), bytes.size()}; msghdr msg{}; msg.msg_iov = &iov; msg.msg_iovlen = 1; msg.msg_control = control.data(); msg.msg_controllen = control.size();
    const auto count = ::recvmsg(socket_fd, &msg, MSG_CMSG_CLOEXEC);
    if (count == 0) throw std::runtime_error("peer closed connection");
    if (count < 0) throw std::system_error(errno, std::generic_category(), "recvmsg");
    if (msg.msg_flags & (MSG_TRUNC | MSG_CTRUNC)) throw std::runtime_error("truncated seqpacket");
    auto packet = decode_packet(std::span(bytes.data(), static_cast<std::size_t>(count)));
    for (auto* cmsg = CMSG_FIRSTHDR(&msg); cmsg; cmsg = CMSG_NXTHDR(&msg, cmsg))
    {
        if (cmsg->cmsg_level == SOL_SOCKET && cmsg->cmsg_type == SCM_RIGHTS)
        {
            const auto fd_count = (cmsg->cmsg_len - CMSG_LEN(0)) / sizeof(int);
            const auto* fds = reinterpret_cast<const int*>(CMSG_DATA(cmsg)); packet.fds.insert(packet.fds.end(), fds, fds + fd_count);
        }
    }
    return packet;
}
}
