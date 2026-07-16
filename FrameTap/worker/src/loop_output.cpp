#include "frametap/worker/loop_output.hpp"

#include <algorithm>
#include <array>
#include <stdexcept>
#include <string>
#include <wayland-client.h>
#include "xdg-shell-client-protocol.h"
#include "linux-dmabuf-unstable-v1-client-protocol.h"

namespace frametap::worker
{
struct LoopOutput::Impl
{
    struct Slot
    {
        Impl* owner = nullptr;
        std::uint32_t id = 0;
        wl_buffer* buffer = nullptr;
        std::uint64_t sequence = 0;
    };

    wl_display* display = nullptr;
    wl_registry* registry = nullptr;
    wl_compositor* compositor = nullptr;
    xdg_wm_base* wm_base = nullptr;
    zwp_linux_dmabuf_v1* dmabuf = nullptr;
    wl_surface* surface = nullptr;
    xdg_surface* xdg_surface_object = nullptr;
    xdg_toplevel* toplevel = nullptr;
    std::array<Slot, protocol::capture_pool_size> slots{};
    Release release;
    std::int32_t width = 0;
    std::int32_t height = 0;
    bool configured = false;
    bool closing = false;
    bool minimize_on_first_frame = false;
    bool minimize_sent = false;

    static void registry_global(void* data, wl_registry* registry, std::uint32_t name,
      const char* interface, std::uint32_t version)
    {
        auto& self = *static_cast<Impl*>(data); const std::string id = interface;
        if (id == wl_compositor_interface.name)
            self.compositor = static_cast<wl_compositor*>(wl_registry_bind(registry, name,
              &wl_compositor_interface, std::min(version, 4U)));
        else if (id == xdg_wm_base_interface.name)
            self.wm_base = static_cast<xdg_wm_base*>(wl_registry_bind(registry, name,
              &xdg_wm_base_interface, std::min(version, 6U)));
        else if (id == zwp_linux_dmabuf_v1_interface.name)
            self.dmabuf = static_cast<zwp_linux_dmabuf_v1*>(wl_registry_bind(registry, name,
              &zwp_linux_dmabuf_v1_interface, std::min(version, 4U)));
    }
    static void registry_remove(void*, wl_registry*, std::uint32_t) {}
    static void ping(void*, xdg_wm_base* base, std::uint32_t serial) { xdg_wm_base_pong(base, serial); }
    static void surface_configure(void* data, xdg_surface* surface, std::uint32_t serial)
    {
        auto& self = *static_cast<Impl*>(data); xdg_surface_ack_configure(surface, serial); self.configured = true;
    }
    static void toplevel_configure(void*, xdg_toplevel*, std::int32_t, std::int32_t, wl_array*) {}
    static void toplevel_close(void* data, xdg_toplevel*) { static_cast<Impl*>(data)->closing = true; }
    static void configure_bounds(void*, xdg_toplevel*, std::int32_t, std::int32_t) {}
    static void wm_capabilities(void*, xdg_toplevel*, wl_array*) {}
    static void buffer_release(void* data, wl_buffer*)
    {
        auto& slot = *static_cast<Slot*>(data);
        if (slot.sequence && slot.owner->release) slot.owner->release(slot.id, slot.sequence);
        slot.sequence = 0;
    }

    static constexpr wl_registry_listener registry_listener{registry_global, registry_remove};
    static constexpr xdg_wm_base_listener wm_listener{ping};
    static constexpr xdg_surface_listener surface_listener{surface_configure};
    static constexpr xdg_toplevel_listener toplevel_listener{
        toplevel_configure, toplevel_close, configure_bounds, wm_capabilities};
    static constexpr wl_buffer_listener buffer_listener{buffer_release};
};

LoopOutput::LoopOutput() : impl(std::make_unique<Impl>()) {}
LoopOutput::~LoopOutput() { close(); }

void LoopOutput::open(const BufferPool& pool, Release release, bool minimized)
{
    close(); impl = std::make_unique<Impl>(); impl->release = std::move(release);
    impl->minimize_on_first_frame = minimized;
    const auto& offer = pool.description(); impl->width = offer.width; impl->height = offer.height;
    impl->display = wl_display_connect(nullptr);
    if (!impl->display) throw std::runtime_error("could not connect FrameTap Output to Wayland");
    impl->registry = wl_display_get_registry(impl->display);
    wl_registry_add_listener(impl->registry, &Impl::registry_listener, impl.get());
    if (wl_display_roundtrip(impl->display) < 0) throw std::runtime_error("Wayland registry roundtrip failed");
    if (!impl->compositor || !impl->wm_base || !impl->dmabuf)
        throw std::runtime_error("Wayland compositor lacks xdg-shell or linux-dmabuf");
    xdg_wm_base_add_listener(impl->wm_base, &Impl::wm_listener, impl.get());

    impl->surface = wl_compositor_create_surface(impl->compositor);
    impl->xdg_surface_object = xdg_wm_base_get_xdg_surface(impl->wm_base, impl->surface);
    xdg_surface_add_listener(impl->xdg_surface_object, &Impl::surface_listener, impl.get());
    impl->toplevel = xdg_surface_get_toplevel(impl->xdg_surface_object);
    xdg_toplevel_add_listener(impl->toplevel, &Impl::toplevel_listener, impl.get());
    xdg_toplevel_set_app_id(impl->toplevel, "io.substr8.frametap.output");
    const std::string title = "FrameTap Output — " + offer.title;
    xdg_toplevel_set_title(impl->toplevel, title.c_str());
    xdg_toplevel_set_min_size(impl->toplevel, offer.width, offer.height);
    xdg_toplevel_set_max_size(impl->toplevel, offer.width, offer.height);

    auto* opaque = wl_compositor_create_region(impl->compositor);
    wl_region_add(opaque, 0, 0, offer.width, offer.height);
    wl_surface_set_opaque_region(impl->surface, opaque);
    wl_region_destroy(opaque);
    wl_surface_commit(impl->surface);
    if (wl_display_roundtrip(impl->display) < 0 || !impl->configured)
        throw std::runtime_error("FrameTap Output was not configured");

    for (std::uint32_t id = 0; id < impl->slots.size(); ++id)
    {
        auto& slot = impl->slots[id]; slot.owner = impl.get(); slot.id = id;
        const auto& record = offer.buffers[id];
        auto* params = zwp_linux_dmabuf_v1_create_params(impl->dmabuf);
        for (std::uint32_t plane = 0; plane < record.plane_count; ++plane)
        {
            zwp_linux_buffer_params_v1_add(params, pool.fd(id, plane), plane,
              record.planes[plane].offset, record.planes[plane].stride,
              static_cast<std::uint32_t>(offer.modifier >> 32U),
              static_cast<std::uint32_t>(offer.modifier & 0xffffffffU));
        }
        slot.buffer = zwp_linux_buffer_params_v1_create_immed(params,
          offer.width, offer.height, offer.format, 0);
        zwp_linux_buffer_params_v1_destroy(params);
        if (!slot.buffer) throw std::runtime_error("failed to import a FrameTap DMA-BUF");
        wl_buffer_add_listener(slot.buffer, &Impl::buffer_listener, &slot);
    }
    wl_display_flush(impl->display);
}

void LoopOutput::present(std::uint32_t buffer_id, std::uint64_t sequence)
{
    if (!ready() || buffer_id >= impl->slots.size()) throw std::runtime_error("invalid output frame");
    auto& slot = impl->slots[buffer_id];
    if (slot.sequence != 0)
        throw std::runtime_error("FrameTap plugin reused a busy output buffer");
    slot.sequence = sequence;
    wl_surface_attach(impl->surface, slot.buffer, 0, 0);
    wl_surface_damage_buffer(impl->surface, 0, 0, impl->width, impl->height);
    wl_surface_commit(impl->surface);
    if (impl->minimize_on_first_frame && !impl->minimize_sent)
    {
        xdg_toplevel_set_minimized(impl->toplevel);
        impl->minimize_sent = true;
    }
    wl_display_flush(impl->display);
}

void LoopOutput::dispatch_pending()
{
    if (impl->display && wl_display_dispatch_pending(impl->display) < 0) impl->closing = true;
}
void LoopOutput::dispatch_readable()
{
    if (impl->display && wl_display_dispatch(impl->display) < 0) impl->closing = true;
}
void LoopOutput::flush() { if (impl->display) wl_display_flush(impl->display); }

void LoopOutput::close() noexcept
{
    if (!impl) return;
    for (auto& slot : impl->slots) if (slot.buffer) { wl_buffer_destroy(slot.buffer); slot.buffer = nullptr; }
    if (impl->toplevel) xdg_toplevel_destroy(impl->toplevel);
    if (impl->xdg_surface_object) xdg_surface_destroy(impl->xdg_surface_object);
    if (impl->surface) wl_surface_destroy(impl->surface);
    if (impl->dmabuf) zwp_linux_dmabuf_v1_destroy(impl->dmabuf);
    if (impl->wm_base) xdg_wm_base_destroy(impl->wm_base);
    if (impl->compositor) wl_compositor_destroy(impl->compositor);
    if (impl->registry) wl_registry_destroy(impl->registry);
    if (impl->display) wl_display_disconnect(impl->display);
    impl = std::make_unique<Impl>();
}

int LoopOutput::fd() const noexcept { return impl->display ? wl_display_get_fd(impl->display) : -1; }
bool LoopOutput::ready() const noexcept { return impl->display && impl->surface && !impl->closing; }
bool LoopOutput::close_requested() const noexcept { return impl->closing; }
}
