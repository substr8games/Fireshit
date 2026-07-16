#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace frametap::protocol
{
constexpr std::uint32_t magic = 0x46544150U;
constexpr std::uint16_t version = 5;
constexpr std::size_t header_size = 32;
constexpr std::size_t max_packet_size = 64U * 1024U;
constexpr std::uint32_t capture_pool_size = 4;
constexpr std::uint32_t max_planes = 4;

struct Header
{
    std::uint32_t magic_value = magic;
    std::uint16_t version_value = version;
    std::uint16_t type = 0;
    std::uint32_t size = header_size;
    std::uint32_t flags = 0;
    std::uint64_t request_id = 0;
    std::uint64_t session_id = 0;
};
static_assert(sizeof(Header) == header_size);

enum class MessageType : std::uint16_t
{
    hello = 1, hello_ok = 2,
    list_views = 10, view_list = 11,
    list_outputs = 12, output_list = 13,
    prepare_session = 20, session_offer = 21,
    start_session = 24, session_started = 25,
    stop_session = 26, session_stopped = 27,
    frame_ready = 30, release_buffer = 31, target_closed = 32,
    error = 0x7fff,
};

enum class ErrorCode : std::uint32_t
{
    malformed_packet = 1, unsupported_version = 2, bad_state = 3,
    busy = 4, target_not_found = 5, target_ambiguous = 6,
    permission_denied = 7, unsupported_format = 8,
    not_implemented = 9, internal = 10,
};

enum class SelectorType : std::uint32_t
{
    focused = 1,
    app_id = 2,
    view_id = 3,
    output = 4,
    region = 5,
};

struct Packet
{
    Header header;
    std::vector<std::uint8_t> payload;
    std::vector<int> fds;
};

struct ViewRecord
{
    std::uint64_t view_id = 0;
    std::int32_t width = 0;
    std::int32_t height = 0;
    bool focused = false;
    bool mapped = false;
    std::uint32_t pid = 0;
    std::string app_id;
    std::string title;
};

struct OutputRecord
{
    std::string name;
    std::int32_t x = 0;
    std::int32_t y = 0;
    std::int32_t width = 0;
    std::int32_t height = 0;
    std::uint32_t scale_milli = 1000;
    bool cursor = false;
};

struct PrepareSession
{
    SelectorType selector_type = SelectorType::focused;
    std::string selector;
    std::uint64_t selected_view_id = 0;
    std::int32_t region_x = 0;
    std::int32_t region_y = 0;
    std::int32_t region_width = 0;
    std::int32_t region_height = 0;
    std::uint32_t requested_fps = 60;
    std::uint32_t pool_size = capture_pool_size;
};

struct PlaneRecord
{
    std::uint32_t stride = 0;
    std::uint32_t offset = 0;
};

struct BufferRecord
{
    std::uint32_t buffer_id = 0;
    std::uint32_t plane_count = 0;
    std::array<PlaneRecord, max_planes> planes{};
};

struct SessionOffer
{
    std::uint64_t target_view_id = 0;
    std::int32_t width = 0;
    std::int32_t height = 0;
    std::uint32_t format = 0;
    std::uint64_t modifier = 0;
    std::string app_id;
    std::string title;
    std::array<BufferRecord, capture_pool_size> buffers{};
};

struct FrameReady
{
    std::uint32_t buffer_id = 0;
    std::uint64_t sequence = 0;
    std::uint64_t capture_time_ns = 0;
};

struct ReleaseBuffer
{
    std::uint32_t buffer_id = 0;
    std::uint64_t sequence = 0;
};

struct SessionStopped
{
    std::uint64_t frames_sent = 0;
    std::uint64_t frames_dropped = 0;
};

struct ErrorMessage
{
    ErrorCode code = ErrorCode::internal;
    std::string message;
};

std::string message_type_name(MessageType type);
}
