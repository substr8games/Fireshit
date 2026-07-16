#include "frametap/plugin/server.hpp"

#include <cmath>
#include <cstdint>
#include <stdexcept>

extern "C"
{
#include <wlr/types/wlr_compositor.h>
#include <wlr/types/wlr_output.h>
}

#include <wayfire/core.hpp>
#include <wayfire/output-layout.hpp>
#include <wayfire/output.hpp>
#include <wayfire/seat.hpp>
#include <wayfire/view.hpp>
#include <wayland-server-core.h>

namespace frametap::plugin
{
namespace
{
std::uint32_t view_pid(const wayfire_view& view) noexcept
{
    if (!view)
    {
        return 0;
    }

    auto* surface = view->get_wlr_surface();
    if (!surface || !surface->resource)
    {
        return 0;
    }

    pid_t pid = 0;
    uid_t uid = 0;
    gid_t gid = 0;
    wl_client_get_credentials(wl_resource_get_client(surface->resource),
      &pid, &uid, &gid);
    return pid > 0 ? static_cast<std::uint32_t>(pid) : 0;
}

bool capturable(const wayfire_view& view)
{
    return view && view->is_mapped() &&
      view->role == wf::VIEW_ROLE_TOPLEVEL &&
      view->get_app_id() != "io.substr8.frametap.output" &&
      view->get_app_id() != "io.substr8.frametap.selector";
}

bool point_inside(const wf::geometry_t& box, const wf::pointf_t& point)
{
    return point.x >= box.x && point.y >= box.y &&
      point.x < box.x + box.width && point.y < box.y + box.height;
}
}

std::vector<protocol::ViewRecord> Server::catalog_views() const
{
    std::vector<protocol::ViewRecord> result;
    const auto focused = wf::get_core().seat->get_active_view();
    for (auto view : wf::get_core().get_all_views())
    {
        if (!capturable(view))
        {
            continue;
        }

        const auto box = view->get_bounding_box();
        protocol::ViewRecord record;
        record.view_id = static_cast<std::uint64_t>(
          reinterpret_cast<std::uintptr_t>(view.get()));
        record.width = box.width;
        record.height = box.height;
        record.focused = focused && focused.get() == view.get();
        record.mapped = true;
        record.pid = view_pid(view);
        record.app_id = view->get_app_id();
        record.title = view->get_title();
        result.push_back(std::move(record));
    }

    return result;
}

std::vector<protocol::OutputRecord> Server::catalog_outputs() const
{
    std::vector<protocol::OutputRecord> result;
    if (!wf::get_core().output_layout)
    {
        return result;
    }

    const auto cursor = wf::get_core().get_cursor_position();
    for (auto* output : wf::get_core().output_layout->get_outputs())
    {
        if (!output || !output->handle)
        {
            continue;
        }

        const auto geometry = output->get_layout_geometry();
        protocol::OutputRecord record;
        record.name = output->handle->name;
        if (record.name.empty())
        {
            continue;
        }
        record.x = geometry.x;
        record.y = geometry.y;
        record.width = geometry.width;
        record.height = geometry.height;
        record.scale_milli = static_cast<std::uint32_t>(
          std::lround(output->handle->scale * 1000.0));
        record.cursor = point_inside(geometry, cursor);
        result.push_back(std::move(record));
    }

    return result;
}

wayfire_view Server::resolve_target(
  const protocol::PrepareSession& request) const
{
    std::vector<wayfire_view> matches;
    if (request.selector_type == protocol::SelectorType::focused)
    {
        auto view = wf::get_core().seat->get_active_view();
        if (capturable(view))
        {
            matches.push_back(view);
        }
    } else
    {
        for (auto view : wf::get_core().get_all_views())
        {
            if (!capturable(view))
            {
                continue;
            }

            const auto id = static_cast<std::uint64_t>(
              reinterpret_cast<std::uintptr_t>(view.get()));
            if ((request.selector_type == protocol::SelectorType::app_id &&
                 view->get_app_id() == request.selector) ||
              (request.selector_type == protocol::SelectorType::view_id &&
                 id == request.selected_view_id))
            {
                matches.push_back(view);
            }
        }
    }

    if (matches.empty())
    {
        throw std::runtime_error("target application was not found");
    }
    if (matches.size() > 1)
    {
        throw std::runtime_error("target app ID is ambiguous; use --view-id");
    }

    return matches.front();
}
}
