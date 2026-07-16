#include "frametap/worker/plugin_client.hpp"
#include "frametap/protocol/codec.hpp"
#include <stdexcept>

namespace frametap::worker
{
PluginClient::PluginClient() : socket(protocol::connect_seqpacket(protocol::default_socket_path()))
{
    auto reply = transact(protocol::make_hello(next_request_id++));
    if (reply.header.type == static_cast<std::uint16_t>(protocol::MessageType::error))
        throw std::runtime_error(protocol::decode_error(reply).message);
    if (reply.header.type != static_cast<std::uint16_t>(protocol::MessageType::hello_ok))
        throw std::runtime_error("FrameTap plugin returned an invalid handshake response");
}

protocol::Packet PluginClient::transact(protocol::Packet request)
{
    const auto expected = request.header.request_id; protocol::send_packet(socket.get(), request);
    auto reply = protocol::receive_packet(socket.get());
    if (reply.header.request_id != expected) throw std::runtime_error("FrameTap response request ID mismatch");
    return reply;
}

std::vector<protocol::ViewRecord> PluginClient::list_views()
{
    auto reply = transact(protocol::make_empty(protocol::MessageType::list_views, next_request_id++));
    if (reply.header.type == static_cast<std::uint16_t>(protocol::MessageType::error))
        throw std::runtime_error(protocol::decode_error(reply).message);
    return protocol::decode_view_list(reply);
}

std::vector<protocol::OutputRecord> PluginClient::list_outputs()
{
    auto reply = transact(protocol::make_empty(
      protocol::MessageType::list_outputs, next_request_id++));
    if (reply.header.type == static_cast<std::uint16_t>(protocol::MessageType::error))
        throw std::runtime_error(protocol::decode_error(reply).message);
    return protocol::decode_output_list(reply);
}

protocol::Packet PluginClient::prepare(const protocol::PrepareSession& request)
{
    auto reply = transact(protocol::make_prepare_session(next_request_id++, request));
    if (reply.header.type == static_cast<std::uint16_t>(protocol::MessageType::error))
        throw std::runtime_error(protocol::decode_error(reply).message);
    return reply;
}

void PluginClient::start(std::uint64_t session_id)
{
    auto reply = transact(protocol::make_empty(protocol::MessageType::start_session, next_request_id++, session_id));
    if (reply.header.type == static_cast<std::uint16_t>(protocol::MessageType::error))
        throw std::runtime_error(protocol::decode_error(reply).message);
    if (reply.header.type != static_cast<std::uint16_t>(protocol::MessageType::session_started))
        throw std::runtime_error("FrameTap plugin did not start the capture session");
}

protocol::SessionStopped PluginClient::stop(std::uint64_t session_id)
{
    const auto request_id = next_request_id++;
    protocol::send_packet(socket.get(), protocol::make_empty(protocol::MessageType::stop_session, request_id, session_id));
    for (;;)
    {
        auto packet = receive();
        if (packet.header.type == static_cast<std::uint16_t>(protocol::MessageType::frame_ready))
        {
            const auto frame = protocol::decode_frame_ready(packet); release(session_id, frame.buffer_id, frame.sequence); continue;
        }
        if (packet.header.type == static_cast<std::uint16_t>(protocol::MessageType::error))
            throw std::runtime_error(protocol::decode_error(packet).message);
        if (packet.header.request_id == request_id &&
            packet.header.type == static_cast<std::uint16_t>(protocol::MessageType::session_stopped))
            return protocol::decode_session_stopped(packet);
    }
}

void PluginClient::release(std::uint64_t session_id, std::uint32_t buffer_id, std::uint64_t sequence)
{
    protocol::send_packet(socket.get(), protocol::make_release_buffer(session_id, {buffer_id, sequence}));
}
protocol::Packet PluginClient::receive() { return protocol::receive_packet(socket.get()); }
int PluginClient::fd() const noexcept { return socket.get(); }
}
