#include "target_request.hpp"

#include "frametap/worker/launch.hpp"
#include "frametap/worker/plugin_client.hpp"
#include "frametap/worker/region_selector.hpp"

#include <algorithm>
#include <chrono>
#include <iostream>
#include <stdexcept>

namespace frametap::worker
{
namespace
{
bool spatial_capture(const CliOptions& options)
{
    return options.selector_type == protocol::SelectorType::output ||
      options.selector_type == protocol::SelectorType::region;
}

protocol::OutputRecord resolve_output(const CliOptions& options,
  PluginClient& client)
{
    const auto outputs = client.list_outputs();
    if (outputs.empty())
        throw std::runtime_error("Wayfire reported no capturable outputs");

    if (!options.output_name.empty())
    {
        const auto match = std::find_if(outputs.begin(), outputs.end(),
          [&options] (const auto& output)
          { return output.name == options.output_name; });
        if (match == outputs.end())
            throw std::runtime_error(
              "capture output was not found: " + options.output_name);
        return *match;
    }

    const auto cursor = std::find_if(outputs.begin(), outputs.end(),
      [] (const auto& output) { return output.cursor; });
    return cursor != outputs.end() ? *cursor : outputs.front();
}
}

AudioMode effective_audio_mode(const CliOptions& options)
{
    if (options.audio != AudioMode::automatic) return options.audio;
    if (!options.launch_command.empty()) return AudioMode::app;
    if (spatial_capture(options)) return AudioMode::system;
    return AudioMode::off;
}

protocol::PrepareSession resolve_request(const CliOptions& options,
  PluginClient& client, std::unique_ptr<LaunchedApplication>& launched,
  const std::vector<std::pair<std::string, std::string>>& environment)
{
    protocol::PrepareSession request;
    if (options.selector_type == protocol::SelectorType::output)
    {
        const auto output = resolve_output(options, client);
        request.selector_type = protocol::SelectorType::output;
        request.selector = output.name;
        std::cerr << "FrameTap output target: " << output.name << " — "
                  << output.width << 'x' << output.height << " logical — scale "
                  << (static_cast<double>(output.scale_milli) / 1000.0) << '\n';
        return request;
    }

    if (options.selector_type == protocol::SelectorType::region)
    {
        const auto output = resolve_output(options, client);
        RegionGeometry region;
        if (options.explicit_region)
        {
            region = *options.explicit_region;
        } else
        {
            const auto selected = RegionSelector{}.select(output.name);
            region = {selected.x, selected.y,
              selected.width, selected.height};
        }

        request.selector_type = protocol::SelectorType::region;
        request.selector = output.name;
        request.region_x = region.x;
        request.region_y = region.y;
        request.region_width = region.width;
        request.region_height = region.height;
        std::cerr << "FrameTap region target: " << output.name << " — "
                  << region.x << ',' << region.y << ' ' <<
                  region.width << 'x' << region.height << " logical\n";
        return request;
    }

    if (options.launch_command.empty())
    {
        request.selector_type = *options.selector_type;
        request.selector = options.selector;
        request.selected_view_id = options.view_id;
        return request;
    }

    const auto baseline = client.list_views();
    launched = std::make_unique<LaunchedApplication>(
      options.launch_command, environment);
    const auto target = launched->wait_for_view(baseline,
      [&client] { return client.list_views(); }, std::chrono::seconds(20));
    request.selector_type = protocol::SelectorType::view_id;
    request.selected_view_id = target.view_id;
    std::cerr << "FrameTap attached to launched PID " << target.pid << " — "
              << target.app_id << " — " << target.title << '\n';
    return request;
}
}
