#include "frametap/plugin/capture_backend.hpp"
#include "frametap/protocol/codec.hpp"

#include <cmath>
#include <stdexcept>
#include <vector>

extern "C"
{
#include <wlr/types/wlr_buffer.h>
#include <wlr/types/wlr_output.h>
}

#include <wayfire/core.hpp>
#include <wayfire/output-layout.hpp>
#include <wayfire/output.hpp>
#include <wayfire/scene-render.hpp>
#include <wayfire/scene.hpp>

namespace frametap::plugin
{
namespace
{
bool same_geometry(const wf::geometry_t& left, const wf::geometry_t& right)
{
    return left.x == right.x && left.y == right.y &&
      left.width == right.width && left.height == right.height;
}

bool contains(const wf::geometry_t& outer, const wf::geometry_t& inner)
{
    return inner.width > 0 && inner.height > 0 && inner.x >= outer.x &&
      inner.y >= outer.y && inner.x + inner.width <= outer.x + outer.width &&
      inner.y + inner.height <= outer.y + outer.height;
}
}

protocol::Packet CaptureBackend::prepare(const protocol::PrepareSession& request,
  wayfire_view selected, std::uint64_t request_id,
  std::uint64_t new_session_id, Emit sender)
{
    stop();
    selector_type = request.selector_type;
    target_view = std::move(selected);
    emit = std::move(sender);
    current_session_id = new_session_id;

    try
    {
        protocol::SessionOffer offer;
        if (view_capture())
        {
            if (!target_view || !target_view->get_output())
            {
                throw std::runtime_error("target has no active Wayfire output");
            }

            source_geometry = target_view->get_surface_root_node()->get_bounding_box();
            output_geometry = target_view->get_output()->get_layout_geometry();
            source_scale = target_view->get_output()->handle->scale;
            offer.target_view_id = static_cast<std::uint64_t>(
              reinterpret_cast<std::uintptr_t>(target_view.get()));
            offer.app_id = target_view->get_app_id();
            offer.title = target_view->get_title();
        } else
        {
            prepare_output_target(request, offer);
        }

        if (source_geometry.width <= 0 || source_geometry.height <= 0 ||
          source_scale <= 0.0f)
        {
            throw std::runtime_error("target has no capturable geometry");
        }

        width = static_cast<std::int32_t>(
          std::ceil(static_cast<double>(source_geometry.width) * source_scale));
        height = static_cast<std::int32_t>(
          std::ceil(static_cast<double>(source_geometry.height) * source_scale));
        pool.allocate(width, height);
        pool.describe(offer);

        auto packet = protocol::make_session_offer(
          request_id, current_session_id, offer);
        pool.append_fds(packet);
        return packet;
    } catch (...)
    {
        stop();
        throw;
    }
}

void CaptureBackend::prepare_output_target(const protocol::PrepareSession& request,
  protocol::SessionOffer& offer)
{
    output_name = request.selector;
    auto* output = current_output();
    if (!output || !output->handle)
    {
        throw std::runtime_error("capture output was not found: " + output_name);
    }

    output_geometry = output->get_layout_geometry();
    source_scale = output->handle->scale;
    if (selector_type == protocol::SelectorType::output)
    {
        source_geometry = output_geometry;
        offer.title = "Output " + output_name;
    } else if (selector_type == protocol::SelectorType::region)
    {
        const wf::geometry_t local_region{request.region_x, request.region_y,
          request.region_width, request.region_height};
        const wf::geometry_t output_local{0, 0,
          output_geometry.width, output_geometry.height};
        if (!contains(output_local, local_region))
        {
            throw std::runtime_error(
              "capture region must be positive and contained by one output");
        }

        source_geometry = {output_geometry.x + local_region.x,
          output_geometry.y + local_region.y,
          local_region.width, local_region.height};
        offer.title = "Region " + output_name + " " +
          std::to_string(local_region.x) + "," +
          std::to_string(local_region.y) + " " +
          std::to_string(local_region.width) + "x" +
          std::to_string(local_region.height);
    } else
    {
        throw std::runtime_error("unsupported FrameTap selector type");
    }

    offer.target_view_id = 0;
    offer.app_id = "output:" + output_name;
}

void CaptureBackend::render(std::uint32_t buffer_id)
{
    std::vector<wf::scene::render_instance_uptr> instances;
    wf::output_t* output = nullptr;
    if (view_capture())
    {
        output = target_view->get_output();
        target_view->get_surface_root_node()->gen_render_instances(
          instances, [] (auto) {}, output);
    } else
    {
        output = current_output();
        wf::get_core().scene()->gen_render_instances(
          instances, [] (auto) {}, output);
    }
    if (!output)
    {
        throw std::runtime_error("capture output disappeared");
    }

    const wf::render_buffer_t buffer{pool.buffer(buffer_id), {width, height}};
    wf::render_target_t render_target{buffer};
    render_target.geometry = source_geometry;
    render_target.scale = source_scale;

    wf::render_pass_params_t params;
    params.background_color = {0, 0, 0, 1};
    params.damage = source_geometry;
    params.target = render_target;
    params.instances = &instances;
    params.flags = wf::RPASS_CLEAR_BACKGROUND;
    wf::render_pass_t::run(params);
}

bool CaptureBackend::view_capture() const noexcept
{
    return selector_type == protocol::SelectorType::focused ||
      selector_type == protocol::SelectorType::app_id ||
      selector_type == protocol::SelectorType::view_id;
}

wf::output_t* CaptureBackend::current_output() const
{
    if (!wf::get_core().output_layout || output_name.empty())
    {
        return nullptr;
    }
    return wf::get_core().output_layout->find_output(output_name);
}

bool CaptureBackend::target_alive() const
{
    if (view_capture())
    {
        return target_view && target_view->is_mapped() &&
          target_view->role == wf::VIEW_ROLE_TOPLEVEL &&
          target_view->get_output();
    }

    auto* output = current_output();
    return output && output->handle;
}

bool CaptureBackend::target_geometry_unchanged() const
{
    if (!target_alive())
    {
        return false;
    }
    if (view_capture())
    {
        const auto current_geometry =
          target_view->get_surface_root_node()->get_bounding_box();
        const auto current_scale = target_view->get_output()->handle->scale;
        return same_geometry(current_geometry, source_geometry) &&
          current_scale == source_scale;
    }

    auto* output = current_output();
    return same_geometry(output->get_layout_geometry(), output_geometry) &&
      output->handle->scale == source_scale;
}
}
