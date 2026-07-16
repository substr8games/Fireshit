#include "frametap/protocol/codec.hpp"
#include "codec_internal.hpp"
#include <limits>
#include <stdexcept>
#include <type_traits>
#include <unistd.h>


namespace frametap::protocol
{
using detail::Reader;
using detail::Writer;
using detail::require_type;
using detail::read_header;
using detail::write_header;

Packet make_empty(MessageType type, std::uint64_t request_id, std::uint64_t session_id)
{
    Packet p; p.header.type = static_cast<std::uint16_t>(type);
    p.header.request_id = request_id; p.header.session_id = session_id; return p;
}

Packet make_hello(std::uint64_t request_id)
{
    auto p = make_empty(MessageType::hello, request_id); Writer w;
    w.value<std::uint32_t>(version); w.value<std::uint32_t>(version);
    w.value<std::uint32_t>(static_cast<std::uint32_t>(::getpid())); w.value<std::uint32_t>(0);
    p.payload = std::move(w.bytes); return p;
}

Packet make_hello_ok(std::uint64_t request_id, std::uint32_t server_pid, std::uint32_t plugin_api_version)
{
    auto p = make_empty(MessageType::hello_ok, request_id); Writer w;
    w.value<std::uint32_t>(version); w.value(server_pid); w.value(plugin_api_version); w.value<std::uint32_t>(0);
    p.payload = std::move(w.bytes); return p;
}

Packet make_prepare_session(std::uint64_t request_id, const PrepareSession& request)
{
    auto p = make_empty(MessageType::prepare_session, request_id); Writer w;
    w.value(request.selector_type); w.string(request.selector);
    w.value(request.selected_view_id); w.value(request.region_x);
    w.value(request.region_y); w.value(request.region_width);
    w.value(request.region_height); w.value(request.requested_fps);
    w.value(request.pool_size); p.payload = std::move(w.bytes); return p;
}

Packet make_session_offer(std::uint64_t request_id, std::uint64_t session_id, const SessionOffer& offer)
{
    auto p = make_empty(MessageType::session_offer, request_id, session_id); Writer w;
    w.value(offer.target_view_id); w.value(offer.width); w.value(offer.height);
    w.value(offer.format); w.value(offer.modifier); w.string(offer.app_id); w.string(offer.title);
    for (const auto& buffer : offer.buffers)
    {
        w.value(buffer.buffer_id); w.value(buffer.plane_count);
        for (std::uint32_t i = 0; i < buffer.plane_count; ++i)
        {
            w.value(buffer.planes[i].stride); w.value(buffer.planes[i].offset);
        }
    }
    p.payload = std::move(w.bytes); return p;
}

Packet make_frame_ready(std::uint64_t session_id, const FrameReady& frame)
{
    auto p = make_empty(MessageType::frame_ready, 0, session_id); Writer w;
    w.value(frame.buffer_id); w.value(frame.sequence); w.value(frame.capture_time_ns);
    p.payload = std::move(w.bytes); return p;
}

Packet make_release_buffer(std::uint64_t session_id, const ReleaseBuffer& release)
{
    auto p = make_empty(MessageType::release_buffer, 0, session_id); Writer w;
    w.value(release.buffer_id); w.value(release.sequence); p.payload = std::move(w.bytes); return p;
}

Packet make_session_stopped(std::uint64_t request_id, std::uint64_t session_id, const SessionStopped& stopped)
{
    auto p = make_empty(MessageType::session_stopped, request_id, session_id); Writer w;
    w.value(stopped.frames_sent); w.value(stopped.frames_dropped); p.payload = std::move(w.bytes); return p;
}

Packet make_error(std::uint64_t request_id, std::uint64_t session_id, ErrorCode code, std::string message)
{
    auto p = make_empty(MessageType::error, request_id, session_id); Writer w;
    w.value(code); w.string(message); p.payload = std::move(w.bytes); return p;
}

std::vector<std::uint8_t> encode_packet(const Packet& packet)
{
    Packet copy = packet; const auto total = header_size + copy.payload.size();
    if (total > max_packet_size) throw std::runtime_error("packet exceeds protocol maximum");
    copy.header.size = static_cast<std::uint32_t>(total); Writer w; write_header(w, copy.header);
    w.bytes.insert(w.bytes.end(), copy.payload.begin(), copy.payload.end()); return w.bytes;
}

Packet decode_packet(std::span<const std::uint8_t> bytes)
{
    if (bytes.size() < header_size || bytes.size() > max_packet_size) throw std::runtime_error("invalid packet size");
    Reader r(bytes); Packet p; p.header = read_header(r);
    if (p.header.magic_value != magic) throw std::runtime_error("bad packet magic");
    if (p.header.version_value != version) throw std::runtime_error("unsupported protocol version");
    if (p.header.size != bytes.size()) throw std::runtime_error("packet size mismatch");
    if (p.header.flags != 0) throw std::runtime_error("reserved flags are nonzero");
    p.payload.assign(bytes.begin() + static_cast<std::ptrdiff_t>(header_size), bytes.end()); return p;
}

PrepareSession decode_prepare_session(const Packet& packet)
{
    require_type(packet, MessageType::prepare_session); Reader r(packet.payload); PrepareSession s;
    s.selector_type = r.value<SelectorType>(); s.selector = r.string();
    s.selected_view_id = r.value<std::uint64_t>();
    s.region_x = r.value<std::int32_t>(); s.region_y = r.value<std::int32_t>();
    s.region_width = r.value<std::int32_t>();
    s.region_height = r.value<std::int32_t>();
    s.requested_fps = r.value<std::uint32_t>();
    s.pool_size = r.value<std::uint32_t>();
    if (r.remaining()) throw std::runtime_error("trailing bytes in prepare-session");
    return s;
}

SessionOffer decode_session_offer(const Packet& packet)
{
    require_type(packet, MessageType::session_offer); Reader r(packet.payload); SessionOffer offer;
    offer.target_view_id = r.value<std::uint64_t>(); offer.width = r.value<std::int32_t>(); offer.height = r.value<std::int32_t>();
    offer.format = r.value<std::uint32_t>(); offer.modifier = r.value<std::uint64_t>(); offer.app_id = r.string(); offer.title = r.string();
    for (auto& buffer : offer.buffers)
    {
        buffer.buffer_id = r.value<std::uint32_t>(); buffer.plane_count = r.value<std::uint32_t>();
        if (buffer.plane_count == 0 || buffer.plane_count > max_planes) throw std::runtime_error("invalid plane count");
        for (std::uint32_t i = 0; i < buffer.plane_count; ++i)
        {
            buffer.planes[i].stride = r.value<std::uint32_t>(); buffer.planes[i].offset = r.value<std::uint32_t>();
        }
    }
    if (r.remaining())
    {
        throw std::runtime_error("trailing bytes in session-offer");
    }

    return offer;
}

FrameReady decode_frame_ready(const Packet& packet)
{
    require_type(packet, MessageType::frame_ready); Reader r(packet.payload); FrameReady frame;
    frame.buffer_id = r.value<std::uint32_t>(); frame.sequence = r.value<std::uint64_t>(); frame.capture_time_ns = r.value<std::uint64_t>();
    if (r.remaining())
    {
        throw std::runtime_error("trailing bytes in frame-ready");
    }

    return frame;
}

ReleaseBuffer decode_release_buffer(const Packet& packet)
{
    require_type(packet, MessageType::release_buffer); Reader r(packet.payload); ReleaseBuffer release;
    release.buffer_id = r.value<std::uint32_t>(); release.sequence = r.value<std::uint64_t>();
    if (r.remaining())
    {
        throw std::runtime_error("trailing bytes in release-buffer");
    }

    return release;
}

SessionStopped decode_session_stopped(const Packet& packet)
{
    require_type(packet, MessageType::session_stopped); Reader r(packet.payload); SessionStopped stopped;
    stopped.frames_sent = r.value<std::uint64_t>(); stopped.frames_dropped = r.value<std::uint64_t>();
    if (r.remaining())
    {
        throw std::runtime_error("trailing bytes in session-stopped");
    }

    return stopped;
}

ErrorMessage decode_error(const Packet& packet)
{
    require_type(packet, MessageType::error); Reader r(packet.payload); ErrorMessage e;
    e.code = r.value<ErrorCode>(); e.message = r.string();
    if (r.remaining())
    {
        throw std::runtime_error("trailing bytes in error");
    }

    return e;
}

}
