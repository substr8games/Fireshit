#include "frametap/worker/region_selector.hpp"
#include "region_selector_impl.hpp"

#include <stdexcept>
#include <utility>
#include <sys/mman.h>
#include <unistd.h>

namespace frametap::worker::detail
{
SelectorClient::SelectorClient(std::string output) :
  requested_output(std::move(output))
{}

SelectorClient::~SelectorClient()
{
    close();
}

SelectedRegion SelectorClient::run()
{
    connect();
    while (!finished)
    {
        if (wl_display_dispatch(display) < 0)
        {
            throw std::runtime_error("region selector lost its Wayland connection");
        }
    }

    dismiss_overlay();
    if (cancelled)
    {
        throw std::runtime_error("region selection cancelled");
    }
    return {requested_output, selected_x, selected_y,
      selected_width, selected_height};
}

void SelectorClient::connect()
{
    display = wl_display_connect(nullptr);
    if (!display)
    {
        throw std::runtime_error("could not connect region selector to Wayland");
    }

    registry = wl_display_get_registry(display);
    wl_registry_add_listener(registry, &registry_listener, this);
    if (wl_display_roundtrip(display) < 0 || wl_display_roundtrip(display) < 0)
    {
        throw std::runtime_error("region selector registry roundtrip failed");
    }
    if (!compositor || !shm || !wm_base || !seat || !pointer)
    {
        throw std::runtime_error(
          "Wayland compositor lacks xdg-shell, wl_shm, or pointer input");
    }

    selected_output = find_output(requested_output);
    if (!selected_output)
    {
        throw std::runtime_error(
          "region selector could not find output: " + requested_output);
    }

    xdg_wm_base_add_listener(wm_base, &wm_listener, this);
    surface = wl_compositor_create_surface(compositor);
    xdg_surface_object = xdg_wm_base_get_xdg_surface(wm_base, surface);
    xdg_surface_add_listener(xdg_surface_object, &surface_listener, this);
    toplevel = xdg_surface_get_toplevel(xdg_surface_object);
    xdg_toplevel_add_listener(toplevel, &toplevel_listener, this);
    xdg_toplevel_set_app_id(toplevel, "io.substr8.frametap.selector");
    xdg_toplevel_set_title(toplevel, "FrameTap Region Selector");
    xdg_toplevel_set_fullscreen(toplevel, selected_output->output);
    wl_surface_commit(surface);

    while (!configured)
    {
        if (wl_display_dispatch(display) < 0)
        {
            throw std::runtime_error("region selector was not configured");
        }
    }
    if (surface_width <= 0 || surface_height <= 0)
    {
        throw std::runtime_error("region selector received invalid output geometry");
    }

    allocate_buffers();
    redraw();
    wl_display_flush(display);
}

SelectorClient::OutputBinding* SelectorClient::find_output(const std::string& name)
{
    for (auto& output : outputs)
    {
        if (output->name == name)
        {
            return output.get();
        }
    }
    return nullptr;
}

void SelectorClient::dismiss_overlay() noexcept
{
    if (!display || !surface)
    {
        return;
    }
    wl_surface_attach(surface, nullptr, 0, 0);
    wl_surface_commit(surface);
    wl_display_flush(display);
    wl_display_roundtrip(display);
}

void SelectorClient::close() noexcept
{
    for (auto& buffer : buffers)
    {
        if (buffer.handle) wl_buffer_destroy(buffer.handle);
        if (buffer.mapping && buffer.mapping != MAP_FAILED)
            ::munmap(buffer.mapping, buffer.bytes);
        buffer = {};
    }
    if (pointer) wl_pointer_destroy(pointer);
    if (keyboard) wl_keyboard_destroy(keyboard);
    if (seat) wl_seat_destroy(seat);
    if (toplevel) xdg_toplevel_destroy(toplevel);
    if (xdg_surface_object) xdg_surface_destroy(xdg_surface_object);
    if (surface) wl_surface_destroy(surface);
    for (auto& output : outputs)
        if (output->output) wl_output_destroy(output->output);
    outputs.clear();
    if (wm_base) xdg_wm_base_destroy(wm_base);
    if (shm) wl_shm_destroy(shm);
    if (compositor) wl_compositor_destroy(compositor);
    if (registry) wl_registry_destroy(registry);
    if (display) wl_display_disconnect(display);
    display = nullptr;
}
}

namespace frametap::worker
{
SelectedRegion RegionSelector::select(const std::string& output_name)
{
    if (output_name.empty())
    {
        throw std::runtime_error("region selector requires a resolved output name");
    }
    return detail::SelectorClient(output_name).run();
}
}
