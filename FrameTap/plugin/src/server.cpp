#include "frametap/plugin/server.hpp"
#include "frametap/protocol/codec.hpp"
#include "frametap/protocol/seqpacket.hpp"

#include <cerrno>
#include <cstring>
#include <filesystem>
#include <stdexcept>
#include <system_error>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/un.h>
#include <wayfire/core.hpp>
#include <wayfire/plugin.hpp>
#include <wayland-server-core.h>

namespace frametap::plugin
{
namespace
{
void close_fd(int& fd)
{
    if (fd >= 0)
    {
        ::close(fd);
        fd = -1;
    }
}

void close_packet_fds(protocol::Packet& packet) noexcept
{
    for (int& fd : packet.fds)
    {
        close_fd(fd);
    }
}
}

void Server::start(wl_event_loop* loop)
{
    socket_path = protocol::default_socket_path();
    std::filesystem::create_directories(socket_path.parent_path());
    ::chmod(socket_path.parent_path().c_str(), 0700);

    std::error_code ignored;
    std::filesystem::remove(socket_path, ignored);

    listen_fd = ::socket(AF_UNIX,
      SOCK_SEQPACKET | SOCK_NONBLOCK | SOCK_CLOEXEC, 0);
    if (listen_fd < 0)
    {
        throw std::system_error(errno, std::generic_category(), "FrameTap socket");
    }

    sockaddr_un address{};
    address.sun_family = AF_UNIX;
    const auto text = socket_path.string();
    if (text.size() >= sizeof(address.sun_path))
    {
        throw std::runtime_error("FrameTap socket path too long");
    }

    std::memcpy(address.sun_path, text.c_str(), text.size() + 1);
    if (::bind(listen_fd, reinterpret_cast<sockaddr*>(&address), sizeof(address)) < 0)
    {
        throw std::system_error(errno, std::generic_category(), "FrameTap bind");
    }

    ::chmod(socket_path.c_str(), 0600);
    if (::listen(listen_fd, 1) < 0)
    {
        throw std::system_error(errno, std::generic_category(), "FrameTap listen");
    }

    listen_source = wl_event_loop_add_fd(
      loop, listen_fd, WL_EVENT_READABLE, &Server::on_accept, this);
    if (!listen_source)
    {
        throw std::runtime_error("FrameTap failed to register Wayland event source");
    }
}

void Server::stop() noexcept
{
    capture.stop();
    if (client_source)
    {
        wl_event_source_remove(client_source);
        client_source = nullptr;
    }

    if (listen_source)
    {
        wl_event_source_remove(listen_source);
        listen_source = nullptr;
    }

    close_fd(client_fd);
    close_fd(listen_fd);
    std::error_code ignored;
    std::filesystem::remove(socket_path, ignored);
}

int Server::on_accept(int, std::uint32_t mask, void* data)
{
    if (mask & (WL_EVENT_HANGUP | WL_EVENT_ERROR))
    {
        return 0;
    }

    return static_cast<Server*>(data)->accept_client();
}

int Server::on_client(int, std::uint32_t mask, void* data)
{
    return static_cast<Server*>(data)->handle_client(mask);
}

int Server::accept_client()
{
    const int incoming = ::accept4(
      listen_fd, nullptr, nullptr, SOCK_NONBLOCK | SOCK_CLOEXEC);
    if (incoming < 0)
    {
        return 0;
    }

    ucred credentials{};
    socklen_t length = sizeof(credentials);
    if (::getsockopt(incoming, SOL_SOCKET, SO_PEERCRED,
      &credentials, &length) < 0 || credentials.uid != ::getuid())
    {
        ::close(incoming);
        return 0;
    }

    if (client_fd >= 0)
    {
        try
        {
            protocol::send_packet(incoming, protocol::make_error(
              0, 0, protocol::ErrorCode::busy,
              "another FrameTap session is connected"));
        } catch (...)
        {}

        ::close(incoming);
        return 0;
    }

    client_fd = incoming;
    client_source = wl_event_loop_add_fd(wf::get_core().ev_loop,
      client_fd, WL_EVENT_READABLE, &Server::on_client, this);
    if (!client_source)
    {
        close_client();
    }

    return 0;
}

int Server::handle_client(std::uint32_t mask)
{
    if (mask & (WL_EVENT_HANGUP | WL_EVENT_ERROR))
    {
        close_client();
        return 0;
    }

    std::uint64_t request_id = 0;
    std::uint64_t request_session_id = capture.session_id();
    protocol::MessageType request_type = protocol::MessageType::error;
    try
    {
        auto request = protocol::receive_packet(client_fd);
        request_id = request.header.request_id;
        request_session_id = request.header.session_id;
        request_type = static_cast<protocol::MessageType>(request.header.type);

        if (request_type == protocol::MessageType::hello)
        {
            reply(protocol::make_hello_ok(request_id,
              static_cast<std::uint32_t>(::getpid()),
              WAYFIRE_API_ABI_VERSION));
        } else if (request_type == protocol::MessageType::list_views)
        {
            reply(protocol::make_view_list(request_id, catalog_views()));
        } else if (request_type == protocol::MessageType::list_outputs)
        {
            reply(protocol::make_output_list(request_id, catalog_outputs()));
        } else if (request_type == protocol::MessageType::prepare_session)
        {
            if (capture.session_id())
            {
                throw std::runtime_error("a capture session is already prepared");
            }

            const auto spec = protocol::decode_prepare_session(request);
            if (spec.pool_size != protocol::capture_pool_size ||
              spec.requested_fps != 60)
            {
                throw std::runtime_error(
                  "FrameTap requires four buffers at 60 fps");
            }

            wayfire_view target;
            if (spec.selector_type == protocol::SelectorType::focused ||
              spec.selector_type == protocol::SelectorType::app_id ||
              spec.selector_type == protocol::SelectorType::view_id)
            {
                target = resolve_target(spec);
            }

            const auto session = next_session_id++;
            reply(capture.prepare(spec, target, request_id, session,
              [this] (const protocol::Packet& packet)
              {
                  reply(packet);
              }));
        } else if (request_type == protocol::MessageType::start_session)
        {
            if (request_session_id != capture.session_id())
            {
                throw std::runtime_error("invalid session ID");
            }

            capture.start(wf::get_core().ev_loop);
            reply(protocol::make_empty(protocol::MessageType::session_started,
              request_id, request_session_id));
        } else if (request_type == protocol::MessageType::release_buffer)
        {
            if (request_session_id == capture.session_id())
            {
                const auto release = protocol::decode_release_buffer(request);
                capture.release(release.buffer_id, release.sequence);
            }
        } else if (request_type == protocol::MessageType::stop_session)
        {
            const auto session = capture.session_id();
            const auto stopped = capture.stop();
            reply(protocol::make_session_stopped(request_id, session, stopped));
        } else
        {
            reply(protocol::make_error(request_id, request_session_id,
              protocol::ErrorCode::bad_state,
              "message is invalid in the current FrameTap state"));
        }
    } catch (const std::exception& error)
    {
        if (request_type == protocol::MessageType::prepare_session)
        {
            capture.stop();
        }

        try
        {
            reply(protocol::make_error(request_id, capture.session_id(),
              protocol::ErrorCode::internal, error.what()));
        } catch (...)
        {}
    }

    return 0;
}

void Server::close_client() noexcept
{
    capture.stop();
    if (client_source)
    {
        wl_event_source_remove(client_source);
        client_source = nullptr;
    }

    close_fd(client_fd);
}


void Server::reply(protocol::Packet packet)
{
    try
    {
        protocol::send_packet(client_fd, packet);
    } catch (...)
    {
        close_packet_fds(packet);
        throw;
    }

    close_packet_fds(packet);
}
}
