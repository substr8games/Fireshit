#pragma once

#include "frametap/worker/region_selector.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include <wayland-client.h>
#include "xdg-shell-client-protocol.h"

namespace frametap::worker::detail
{
class SelectorClient
{
  public:
    explicit SelectorClient(std::string requested_output);
    ~SelectorClient();
    SelectedRegion run();

  private:
    struct OutputBinding
    {
        SelectorClient* owner = nullptr;
        wl_output* output = nullptr;
        std::string name;
        std::int32_t scale = 1;
    };

    struct Buffer
    {
        SelectorClient* owner = nullptr;
        wl_buffer* handle = nullptr;
        void* mapping = nullptr;
        std::size_t bytes = 0;
        bool busy = false;
    };

    void connect();
    OutputBinding* find_output(const std::string& name);
    void allocate_buffers();
    void redraw();
    void draw_border(std::uint32_t* pixels, std::int32_t x, std::int32_t y,
      std::int32_t width, std::int32_t height);
    void draw_crosshair(std::uint32_t* pixels);
    void finish_selection();
    void dismiss_overlay() noexcept;
    void close() noexcept;

    static int create_shm(std::size_t bytes);
    static void registry_global(void*, wl_registry*, std::uint32_t,
      const char*, std::uint32_t);
    static void registry_remove(void*, wl_registry*, std::uint32_t);
    static void wm_ping(void*, xdg_wm_base*, std::uint32_t);
    static void xdg_configure(void*, xdg_surface*, std::uint32_t);
    static void toplevel_configure(void*, xdg_toplevel*, std::int32_t,
      std::int32_t, wl_array*);
    static void toplevel_close(void*, xdg_toplevel*);
    static void configure_bounds(void*, xdg_toplevel*, std::int32_t, std::int32_t);
    static void wm_capabilities(void*, xdg_toplevel*, wl_array*);
    static void output_geometry(void*, wl_output*, std::int32_t, std::int32_t,
      std::int32_t, std::int32_t, std::int32_t, const char*, const char*,
      std::int32_t);
    static void output_mode(void*, wl_output*, std::uint32_t, std::int32_t,
      std::int32_t, std::int32_t);
    static void output_done(void*, wl_output*);
    static void output_scale(void*, wl_output*, std::int32_t);
    static void output_name(void*, wl_output*, const char*);
    static void output_description(void*, wl_output*, const char*);
    static void seat_capabilities(void*, wl_seat*, std::uint32_t);
    static void seat_name(void*, wl_seat*, const char*);
    static void pointer_enter(void*, wl_pointer*, std::uint32_t, wl_surface*,
      wl_fixed_t, wl_fixed_t);
    static void pointer_leave(void*, wl_pointer*, std::uint32_t, wl_surface*);
    static void pointer_motion(void*, wl_pointer*, std::uint32_t,
      wl_fixed_t, wl_fixed_t);
    static void pointer_button(void*, wl_pointer*, std::uint32_t,
      std::uint32_t, std::uint32_t, std::uint32_t);
    static void pointer_axis(void*, wl_pointer*, std::uint32_t,
      std::uint32_t, wl_fixed_t);
    static void pointer_frame(void*, wl_pointer*);
    static void pointer_axis_source(void*, wl_pointer*, std::uint32_t);
    static void pointer_axis_stop(void*, wl_pointer*, std::uint32_t,
      std::uint32_t);
    static void pointer_axis_discrete(void*, wl_pointer*, std::uint32_t,
      std::int32_t);
    static void keyboard_keymap(void*, wl_keyboard*, std::uint32_t, int,
      std::uint32_t);
    static void keyboard_enter(void*, wl_keyboard*, std::uint32_t, wl_surface*,
      wl_array*);
    static void keyboard_leave(void*, wl_keyboard*, std::uint32_t, wl_surface*);
    static void keyboard_key(void*, wl_keyboard*, std::uint32_t,
      std::uint32_t, std::uint32_t, std::uint32_t);
    static void keyboard_modifiers(void*, wl_keyboard*, std::uint32_t,
      std::uint32_t, std::uint32_t, std::uint32_t, std::uint32_t);
    static void keyboard_repeat(void*, wl_keyboard*, std::int32_t, std::int32_t);
    static void buffer_release(void*, wl_buffer*);

    static const wl_registry_listener registry_listener;
    static const xdg_wm_base_listener wm_listener;
    static const xdg_surface_listener surface_listener;
    static const xdg_toplevel_listener toplevel_listener;
    static const wl_output_listener output_listener;
    static const wl_seat_listener seat_listener;
    static const wl_pointer_listener pointer_listener;
    static const wl_keyboard_listener keyboard_listener;
    static const wl_buffer_listener buffer_listener;

    std::string requested_output;
    wl_display* display = nullptr;
    wl_registry* registry = nullptr;
    wl_compositor* compositor = nullptr;
    wl_shm* shm = nullptr;
    xdg_wm_base* wm_base = nullptr;
    wl_seat* seat = nullptr;
    wl_pointer* pointer = nullptr;
    wl_keyboard* keyboard = nullptr;
    std::vector<std::unique_ptr<OutputBinding>> outputs;
    OutputBinding* selected_output = nullptr;
    wl_surface* surface = nullptr;
    xdg_surface* xdg_surface_object = nullptr;
    xdg_toplevel* toplevel = nullptr;
    std::array<Buffer, 2> buffers{};
    std::int32_t surface_width = 0;
    std::int32_t surface_height = 0;
    std::int32_t pointer_x = 0;
    std::int32_t pointer_y = 0;
    std::int32_t anchor_x = 0;
    std::int32_t anchor_y = 0;
    std::int32_t selected_x = 0;
    std::int32_t selected_y = 0;
    std::int32_t selected_width = 0;
    std::int32_t selected_height = 0;
    bool configured = false;
    bool dragging = false;
    bool redraw_pending = false;
    bool finished = false;
    bool cancelled = false;
};
}
