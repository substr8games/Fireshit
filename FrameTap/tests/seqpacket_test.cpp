#include "frametap/protocol/codec.hpp"
#include "frametap/protocol/seqpacket.hpp"

#include <cassert>
#include <cstdint>
#include <fcntl.h>
#include <unistd.h>
#include <sys/socket.h>

int main()
{
    using namespace frametap::protocol;
    int sockets[2] = {-1, -1};
    int pipe_fds[2] = {-1, -1};
    assert(::socketpair(AF_UNIX, SOCK_SEQPACKET | SOCK_CLOEXEC, 0, sockets) == 0);
    assert(::pipe2(pipe_fds, O_CLOEXEC) == 0);

    auto packet = make_empty(MessageType::session_offer, 77, 3);
    packet.fds.push_back(pipe_fds[0]);
    send_packet(sockets[0], packet);
    ::close(pipe_fds[0]);

    const std::uint8_t marker = 0xa5;
    assert(::write(pipe_fds[1], &marker, sizeof(marker)) == sizeof(marker));
    auto received = receive_packet(sockets[1]);
    assert(received.header.request_id == 77);
    assert(received.header.session_id == 3);
    assert(received.fds.size() == 1);

    std::uint8_t readback = 0;
    assert(::read(received.fds[0], &readback, sizeof(readback)) == sizeof(readback));
    assert(readback == marker);

    ::close(received.fds[0]);
    ::close(pipe_fds[1]);
    ::close(sockets[0]);
    ::close(sockets[1]);
    return 0;
}
