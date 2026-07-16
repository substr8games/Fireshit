#pragma once

#include <array>
#include <cstdint>
#include <functional>
#include <string>

#include <wayfire/render.hpp>
#include <wayfire/view.hpp>
#include <wayland-server-core.h>

#include "frametap/plugin/linear_buffer_pool.hpp"
#include "frametap/protocol/packet.hpp"

namespace wf { class output_t; }

namespace frametap::plugin
{
class CaptureBackend
{
  public:
    using Emit = std::function<void(const protocol::Packet&)>;

    protocol::Packet prepare(const protocol::PrepareSession& request,
      wayfire_view target, std::uint64_t request_id,
      std::uint64_t session_id, Emit emit);
    void start(wl_event_loop* loop);
    protocol::SessionStopped stop() noexcept;
    void release(std::uint32_t buffer_id, std::uint64_t sequence);
    bool active() const noexcept;
    std::uint64_t session_id() const noexcept;

  private:
    struct SlotState
    {
        bool in_use = false;
        std::uint64_t sequence = 0;
    };

    static int on_tick(void* data) noexcept;
    int tick();
    void schedule_next_tick();
    void fail_session(const char* reason) noexcept;
    void prepare_output_target(const protocol::PrepareSession& request,
      protocol::SessionOffer& offer);
    void render(std::uint32_t buffer_id);
    bool target_alive() const;
    bool target_geometry_unchanged() const;
    bool view_capture() const noexcept;
    wf::output_t* current_output() const;

    LinearBufferPool pool;
    std::array<SlotState, protocol::capture_pool_size> slots;
    protocol::SelectorType selector_type = protocol::SelectorType::focused;
    wayfire_view target_view;
    std::string output_name;
    Emit emit;
    wl_event_source* timer = nullptr;
    std::uint64_t current_session_id = 0;
    std::uint64_t capture_epoch_ns = 0;
    std::uint64_t next_frame_index = 0;
    std::uint64_t frames_sent = 0;
    std::uint64_t frames_dropped = 0;
    wf::geometry_t source_geometry{0, 0, 0, 0};
    wf::geometry_t output_geometry{0, 0, 0, 0};
    float source_scale = 1.0f;
    std::int32_t width = 0;
    std::int32_t height = 0;
    bool running = false;
};
}
