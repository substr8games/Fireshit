#include "region_selector_impl.hpp"

#include <algorithm>
#include <linux/input-event-codes.h>
#include <string>
#include <unistd.h>

namespace frametap::worker::detail
{
void SelectorClient::registry_global(void* data, wl_registry* registry,
  std::uint32_t name, const char* interface, std::uint32_t version)
{
    auto& self = *static_cast<SelectorClient*>(data);
    const std::string id = interface;
    if (id == wl_compositor_interface.name)
        self.compositor = static_cast<wl_compositor*>(wl_registry_bind(
          registry, name, &wl_compositor_interface, std::min(version, 4U)));
    else if (id == wl_shm_interface.name)
        self.shm = static_cast<wl_shm*>(wl_registry_bind(
          registry, name, &wl_shm_interface, 1));
    else if (id == xdg_wm_base_interface.name)
        self.wm_base = static_cast<xdg_wm_base*>(wl_registry_bind(
          registry, name, &xdg_wm_base_interface, std::min(version, 6U)));
    else if (id == wl_seat_interface.name)
    {
        self.seat = static_cast<wl_seat*>(wl_registry_bind(
          registry, name, &wl_seat_interface, std::min(version, 7U)));
        wl_seat_add_listener(self.seat, &seat_listener, &self);
    } else if (id == wl_output_interface.name)
    {
        auto output = std::make_unique<OutputBinding>();
        output->owner = &self;
        output->output = static_cast<wl_output*>(wl_registry_bind(
          registry, name, &wl_output_interface, std::min(version, 4U)));
        wl_output_add_listener(output->output, &output_listener, output.get());
        self.outputs.push_back(std::move(output));
    }
}

void SelectorClient::registry_remove(void*, wl_registry*, std::uint32_t) {}
void SelectorClient::wm_ping(void*, xdg_wm_base* base, std::uint32_t serial)
{ xdg_wm_base_pong(base, serial); }

void SelectorClient::xdg_configure(void* data, xdg_surface* surface,
  std::uint32_t serial)
{
    auto& self = *static_cast<SelectorClient*>(data);
    xdg_surface_ack_configure(surface, serial);
    self.configured = self.surface_width > 0 && self.surface_height > 0;
}

void SelectorClient::toplevel_configure(void* data, xdg_toplevel*,
  std::int32_t width, std::int32_t height, wl_array*)
{
    auto& self = *static_cast<SelectorClient*>(data);
    if (width > 0 && height > 0)
    {
        self.surface_width = width;
        self.surface_height = height;
    }
}

void SelectorClient::toplevel_close(void* data, xdg_toplevel*)
{
    auto& self = *static_cast<SelectorClient*>(data);
    self.cancelled = true;
    self.finished = true;
}
void SelectorClient::configure_bounds(void*, xdg_toplevel*, std::int32_t,
  std::int32_t) {}
void SelectorClient::wm_capabilities(void*, xdg_toplevel*, wl_array*) {}
void SelectorClient::output_geometry(void*, wl_output*, std::int32_t,
  std::int32_t, std::int32_t, std::int32_t, std::int32_t, const char*,
  const char*, std::int32_t) {}
void SelectorClient::output_mode(void*, wl_output*, std::uint32_t,
  std::int32_t, std::int32_t, std::int32_t) {}
void SelectorClient::output_done(void*, wl_output*) {}
void SelectorClient::output_scale(void* data, wl_output*, std::int32_t scale)
{ static_cast<OutputBinding*>(data)->scale = std::max(scale, 1); }
void SelectorClient::output_name(void* data, wl_output*, const char* name)
{ static_cast<OutputBinding*>(data)->name = name ? name : ""; }
void SelectorClient::output_description(void*, wl_output*, const char*) {}

void SelectorClient::seat_capabilities(void* data, wl_seat* seat,
  std::uint32_t capabilities)
{
    auto& self = *static_cast<SelectorClient*>(data);
    if ((capabilities & WL_SEAT_CAPABILITY_POINTER) && !self.pointer)
    {
        self.pointer = wl_seat_get_pointer(seat);
        wl_pointer_add_listener(self.pointer, &pointer_listener, &self);
    }
    if ((capabilities & WL_SEAT_CAPABILITY_KEYBOARD) && !self.keyboard)
    {
        self.keyboard = wl_seat_get_keyboard(seat);
        wl_keyboard_add_listener(self.keyboard, &keyboard_listener, &self);
    }
}
void SelectorClient::seat_name(void*, wl_seat*, const char*) {}

void SelectorClient::pointer_enter(void* data, wl_pointer*, std::uint32_t,
  wl_surface*, wl_fixed_t x, wl_fixed_t y)
{
    auto& self = *static_cast<SelectorClient*>(data);
    self.pointer_x = wl_fixed_to_int(x);
    self.pointer_y = wl_fixed_to_int(y);
    self.redraw();
}
void SelectorClient::pointer_leave(void*, wl_pointer*, std::uint32_t,
  wl_surface*) {}
void SelectorClient::pointer_motion(void* data, wl_pointer*, std::uint32_t,
  wl_fixed_t x, wl_fixed_t y)
{
    auto& self = *static_cast<SelectorClient*>(data);
    self.pointer_x = wl_fixed_to_int(x);
    self.pointer_y = wl_fixed_to_int(y);
    self.redraw();
}

void SelectorClient::pointer_button(void* data, wl_pointer*, std::uint32_t,
  std::uint32_t, std::uint32_t button, std::uint32_t state)
{
    auto& self = *static_cast<SelectorClient*>(data);
    if (button == BTN_RIGHT && state == WL_POINTER_BUTTON_STATE_PRESSED)
    {
        self.cancelled = true;
        self.finished = true;
    } else if (button == BTN_LEFT && state == WL_POINTER_BUTTON_STATE_PRESSED)
    {
        self.dragging = true;
        self.anchor_x = self.pointer_x;
        self.anchor_y = self.pointer_y;
        self.redraw();
    } else if (button == BTN_LEFT && state == WL_POINTER_BUTTON_STATE_RELEASED &&
      self.dragging)
    {
        self.finish_selection();
    }
}
void SelectorClient::pointer_axis(void*, wl_pointer*, std::uint32_t,
  std::uint32_t, wl_fixed_t) {}
void SelectorClient::pointer_frame(void*, wl_pointer*) {}
void SelectorClient::pointer_axis_source(void*, wl_pointer*, std::uint32_t) {}
void SelectorClient::pointer_axis_stop(void*, wl_pointer*, std::uint32_t,
  std::uint32_t) {}
void SelectorClient::pointer_axis_discrete(void*, wl_pointer*, std::uint32_t,
  std::int32_t) {}

void SelectorClient::keyboard_keymap(void*, wl_keyboard*, std::uint32_t, int fd,
  std::uint32_t) { if (fd >= 0) ::close(fd); }
void SelectorClient::keyboard_enter(void*, wl_keyboard*, std::uint32_t,
  wl_surface*, wl_array*) {}
void SelectorClient::keyboard_leave(void*, wl_keyboard*, std::uint32_t,
  wl_surface*) {}
void SelectorClient::keyboard_key(void* data, wl_keyboard*, std::uint32_t,
  std::uint32_t, std::uint32_t key, std::uint32_t state)
{
    auto& self = *static_cast<SelectorClient*>(data);
    if (key == KEY_ESC && state == WL_KEYBOARD_KEY_STATE_PRESSED)
    {
        self.cancelled = true;
        self.finished = true;
    }
}
void SelectorClient::keyboard_modifiers(void*, wl_keyboard*, std::uint32_t,
  std::uint32_t, std::uint32_t, std::uint32_t, std::uint32_t) {}
void SelectorClient::keyboard_repeat(void*, wl_keyboard*, std::int32_t,
  std::int32_t) {}

void SelectorClient::buffer_release(void* data, wl_buffer*)
{
    auto& buffer = *static_cast<Buffer*>(data);
    buffer.busy = false;
    if (buffer.owner->redraw_pending && !buffer.owner->finished)
    {
        buffer.owner->redraw();
    }
}

const wl_registry_listener SelectorClient::registry_listener{
  registry_global, registry_remove};
const xdg_wm_base_listener SelectorClient::wm_listener{wm_ping};
const xdg_surface_listener SelectorClient::surface_listener{xdg_configure};
const xdg_toplevel_listener SelectorClient::toplevel_listener{
  toplevel_configure, toplevel_close, configure_bounds, wm_capabilities};
const wl_output_listener SelectorClient::output_listener{
  output_geometry, output_mode, output_done, output_scale,
  output_name, output_description};
const wl_seat_listener SelectorClient::seat_listener{seat_capabilities, seat_name};
const wl_pointer_listener SelectorClient::pointer_listener{
  pointer_enter, pointer_leave, pointer_motion, pointer_button,
  pointer_axis, pointer_frame, pointer_axis_source, pointer_axis_stop,
  pointer_axis_discrete};
const wl_keyboard_listener SelectorClient::keyboard_listener{
  keyboard_keymap, keyboard_enter, keyboard_leave, keyboard_key,
  keyboard_modifiers, keyboard_repeat};
const wl_buffer_listener SelectorClient::buffer_listener{buffer_release};
}
