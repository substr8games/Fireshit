#pragma once

#include <span>
#include <string>
#include <vector>
#include "frametap/protocol/packet.hpp"

namespace frametap::protocol
{
Packet make_empty(MessageType type, std::uint64_t request_id = 0, std::uint64_t session_id = 0);
Packet make_hello(std::uint64_t request_id);
Packet make_hello_ok(std::uint64_t request_id, std::uint32_t server_pid, std::uint32_t plugin_api_version);
Packet make_view_list(std::uint64_t request_id, const std::vector<ViewRecord>& views);
Packet make_output_list(std::uint64_t request_id, const std::vector<OutputRecord>& outputs);
Packet make_prepare_session(std::uint64_t request_id, const PrepareSession& request);
Packet make_session_offer(std::uint64_t request_id, std::uint64_t session_id, const SessionOffer& offer);
Packet make_frame_ready(std::uint64_t session_id, const FrameReady& frame);
Packet make_release_buffer(std::uint64_t session_id, const ReleaseBuffer& release);
Packet make_session_stopped(std::uint64_t request_id, std::uint64_t session_id, const SessionStopped& stopped);
Packet make_error(std::uint64_t request_id, std::uint64_t session_id, ErrorCode code, std::string message);

std::vector<std::uint8_t> encode_packet(const Packet& packet);
Packet decode_packet(std::span<const std::uint8_t> bytes);
std::vector<ViewRecord> decode_view_list(const Packet& packet);
std::vector<OutputRecord> decode_output_list(const Packet& packet);
PrepareSession decode_prepare_session(const Packet& packet);
SessionOffer decode_session_offer(const Packet& packet);
FrameReady decode_frame_ready(const Packet& packet);
ReleaseBuffer decode_release_buffer(const Packet& packet);
SessionStopped decode_session_stopped(const Packet& packet);
ErrorMessage decode_error(const Packet& packet);
}
