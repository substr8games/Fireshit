#include "frametap/protocol/codec.hpp"
#include "codec_internal.hpp"

#include <stdexcept>

namespace frametap::protocol
{
using detail::Reader;
using detail::Writer;
using detail::require_type;

Packet make_view_list(std::uint64_t request_id,
  const std::vector<ViewRecord>& views)
{
    auto packet = make_empty(MessageType::view_list, request_id);
    Writer writer;
    writer.value<std::uint32_t>(static_cast<std::uint32_t>(views.size()));
    for (const auto& view : views)
    {
        writer.value(view.view_id);
        writer.value(view.width);
        writer.value(view.height);
        writer.value<std::uint32_t>(
          (view.focused ? 1U : 0U) | (view.mapped ? 2U : 0U));
        writer.value(view.pid);
        writer.string(view.app_id);
        writer.string(view.title);
    }
    packet.payload = std::move(writer.bytes);
    return packet;
}

Packet make_output_list(std::uint64_t request_id,
  const std::vector<OutputRecord>& outputs)
{
    auto packet = make_empty(MessageType::output_list, request_id);
    Writer writer;
    writer.value<std::uint32_t>(static_cast<std::uint32_t>(outputs.size()));
    for (const auto& output : outputs)
    {
        writer.string(output.name);
        writer.value(output.x);
        writer.value(output.y);
        writer.value(output.width);
        writer.value(output.height);
        writer.value(output.scale_milli);
        writer.value<std::uint32_t>(output.cursor ? 1U : 0U);
    }
    packet.payload = std::move(writer.bytes);
    return packet;
}

std::vector<ViewRecord> decode_view_list(const Packet& packet)
{
    require_type(packet, MessageType::view_list);
    Reader reader(packet.payload);
    const auto count = reader.value<std::uint32_t>();
    std::vector<ViewRecord> views;
    views.reserve(count);
    for (std::uint32_t index = 0; index < count; ++index)
    {
        ViewRecord view;
        view.view_id = reader.value<std::uint64_t>();
        view.width = reader.value<std::int32_t>();
        view.height = reader.value<std::int32_t>();
        const auto flags = reader.value<std::uint32_t>();
        view.focused = flags & 1U;
        view.mapped = flags & 2U;
        view.pid = reader.value<std::uint32_t>();
        view.app_id = reader.string();
        view.title = reader.string();
        views.push_back(std::move(view));
    }
    if (reader.remaining())
    {
        throw std::runtime_error("trailing bytes in view-list");
    }
    return views;
}

std::vector<OutputRecord> decode_output_list(const Packet& packet)
{
    require_type(packet, MessageType::output_list);
    Reader reader(packet.payload);
    const auto count = reader.value<std::uint32_t>();
    std::vector<OutputRecord> outputs;
    outputs.reserve(count);
    for (std::uint32_t index = 0; index < count; ++index)
    {
        OutputRecord output;
        output.name = reader.string();
        output.x = reader.value<std::int32_t>();
        output.y = reader.value<std::int32_t>();
        output.width = reader.value<std::int32_t>();
        output.height = reader.value<std::int32_t>();
        output.scale_milli = reader.value<std::uint32_t>();
        output.cursor = reader.value<std::uint32_t>() & 1U;
        outputs.push_back(std::move(output));
    }
    if (reader.remaining())
    {
        throw std::runtime_error("trailing bytes in output-list");
    }
    return outputs;
}

std::string message_type_name(MessageType type)
{
    switch (type)
    {
      case MessageType::hello: return "HELLO";
      case MessageType::hello_ok: return "HELLO_OK";
      case MessageType::list_views: return "LIST_VIEWS";
      case MessageType::view_list: return "VIEW_LIST";
      case MessageType::list_outputs: return "LIST_OUTPUTS";
      case MessageType::output_list: return "OUTPUT_LIST";
      case MessageType::prepare_session: return "PREPARE_SESSION";
      case MessageType::session_offer: return "SESSION_OFFER";
      case MessageType::start_session: return "START_SESSION";
      case MessageType::session_started: return "SESSION_STARTED";
      case MessageType::stop_session: return "STOP_SESSION";
      case MessageType::session_stopped: return "SESSION_STOPPED";
      case MessageType::frame_ready: return "FRAME_READY";
      case MessageType::release_buffer: return "RELEASE_BUFFER";
      case MessageType::target_closed: return "TARGET_CLOSED";
      case MessageType::error: return "ERROR";
    }
    return "UNKNOWN";
}
}
