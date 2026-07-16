#include "frametap/plugin/capture_backend.hpp"
#include "frametap/protocol/codec.hpp"

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <limits>
#include <stdexcept>

namespace frametap::plugin
{
namespace
{
constexpr std::uint64_t nanoseconds_per_second = 1'000'000'000ULL;
constexpr std::uint64_t nanoseconds_per_millisecond = 1'000'000ULL;
constexpr std::uint64_t capture_fps = 60ULL;

std::uint64_t monotonic_ns()
{
    return static_cast<std::uint64_t>(std::chrono::duration_cast<std::chrono::nanoseconds>(
      std::chrono::steady_clock::now().time_since_epoch()).count());
}

std::uint64_t frame_deadline_ns(std::uint64_t epoch_ns,
  std::uint64_t frame_index)
{
    return epoch_ns +
      ((frame_index * nanoseconds_per_second) + capture_fps - 1) / capture_fps;
}

std::uint64_t due_frame_index(std::uint64_t epoch_ns, std::uint64_t now_ns)
{
    if (now_ns <= epoch_ns)
    {
        return 0;
    }

    return ((now_ns - epoch_ns) * capture_fps) / nanoseconds_per_second;
}
}

void CaptureBackend::start(wl_event_loop* loop)
{
    if (current_session_id == 0 || !target_alive())
    {
        throw std::runtime_error("capture session is not prepared");
    }

    if (!timer)
    {
        timer = wl_event_loop_add_timer(loop, &CaptureBackend::on_tick, this);
    }
    if (!timer)
    {
        throw std::runtime_error("failed to create capture timer");
    }

    capture_epoch_ns = monotonic_ns();
    next_frame_index = 0;
    running = true;
    schedule_next_tick();
}

protocol::SessionStopped CaptureBackend::stop() noexcept
{
    running = false;
    if (timer)
    {
        wl_event_source_remove(timer);
        timer = nullptr;
    }

    pool.release();
    slots = {};
    selector_type = protocol::SelectorType::focused;
    target_view.reset();
    output_name.clear();
    emit = {};
    current_session_id = 0;
    capture_epoch_ns = 0;
    next_frame_index = 0;
    source_geometry = {0, 0, 0, 0};
    output_geometry = {0, 0, 0, 0};
    source_scale = 1.0f;
    width = 0;
    height = 0;

    const protocol::SessionStopped result{frames_sent, frames_dropped};
    frames_sent = 0;
    frames_dropped = 0;
    return result;
}

void CaptureBackend::release(std::uint32_t buffer_id, std::uint64_t sequence)
{
    if (buffer_id >= slots.size())
    {
        return;
    }

    auto& slot = slots[buffer_id];
    if (slot.in_use && slot.sequence == sequence)
    {
        slot.in_use = false;
    }
}

bool CaptureBackend::active() const noexcept
{
    return running;
}

std::uint64_t CaptureBackend::session_id() const noexcept
{
    return current_session_id;
}

int CaptureBackend::on_tick(void* data) noexcept
{
    auto* capture = static_cast<CaptureBackend*>(data);
    try
    {
        return capture->tick();
    } catch (const std::exception& error)
    {
        capture->fail_session(error.what());
    } catch (...)
    {
        capture->fail_session("unknown compositor capture failure");
    }
    return 0;
}

int CaptureBackend::tick()
{
    if (!running)
    {
        return 0;
    }

    if (!target_alive() || !target_geometry_unchanged())
    {
        try
        {
            emit(protocol::make_empty(
              protocol::MessageType::target_closed, 0, current_session_id));
        } catch (...)
        {}
        running = false;
        return 0;
    }

    const auto due_index = due_frame_index(capture_epoch_ns, monotonic_ns());
    if (due_index < next_frame_index)
    {
        schedule_next_tick();
        return 0;
    }
    if (due_index > next_frame_index)
    {
        frames_dropped += due_index - next_frame_index;
        next_frame_index = due_index;
    }

    const auto frame_index = next_frame_index;
    std::uint32_t buffer_id = slots.size();
    for (std::uint32_t index = 0; index < slots.size(); ++index)
    {
        if (!slots[index].in_use)
        {
            buffer_id = index;
            break;
        }
    }

    if (buffer_id == slots.size())
    {
        ++frames_dropped;
    } else
    {
        auto& slot = slots[buffer_id];
        render(buffer_id);
        slot.in_use = true;
        slot.sequence = frame_index + 1;
        auto packet = protocol::make_frame_ready(current_session_id,
          {buffer_id, slot.sequence,
            frame_deadline_ns(capture_epoch_ns, frame_index)});
        packet.fds.push_back(pool.export_read_fence(buffer_id));
        emit(packet);
        ++frames_sent;
    }

    ++next_frame_index;
    schedule_next_tick();
    return 0;
}

void CaptureBackend::schedule_next_tick()
{
    if (!running || !timer)
    {
        return;
    }

    const auto now_ns = monotonic_ns();
    const auto deadline_ns = frame_deadline_ns(capture_epoch_ns, next_frame_index);
    const auto remaining_ns = deadline_ns > now_ns ? deadline_ns - now_ns : 0;
    const auto rounded_ms = (remaining_ns + nanoseconds_per_millisecond - 1) /
      nanoseconds_per_millisecond;
    const auto bounded_ms = std::min<std::uint64_t>(
      std::max<std::uint64_t>(rounded_ms, 1),
      static_cast<std::uint64_t>(std::numeric_limits<int>::max()));
    wl_event_source_timer_update(timer, static_cast<int>(bounded_ms));
}

void CaptureBackend::fail_session(const char* reason) noexcept
{
    running = false;
    std::fprintf(stderr, "FrameTap plugin: capture session stopped: %s\n",
      reason ? reason : "unknown failure");
    if (!emit || current_session_id == 0)
    {
        return;
    }

    try
    {
        emit(protocol::make_error(0, current_session_id,
          protocol::ErrorCode::internal,
          reason ? reason : "unknown compositor capture failure"));
    } catch (...)
    {}
}
}
