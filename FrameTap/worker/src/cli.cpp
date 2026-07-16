#include "frametap/worker/cli.hpp"

#include <array>
#include <charconv>
#include <limits>
#include <stdexcept>
#include <string_view>

namespace frametap::worker
{
namespace
{
std::uint64_t parse_u64(const std::string& value)
{
    std::uint64_t out = 0;
    const auto result = std::from_chars(value.data(), value.data() + value.size(), out);
    if (result.ec != std::errc{} || result.ptr != value.data() + value.size())
        throw std::runtime_error("invalid view ID: " + value);
    return out;
}

std::int32_t parse_i32(std::string_view value, const char* field)
{
    std::int64_t out = 0;
    const auto result = std::from_chars(value.data(), value.data() + value.size(), out);
    if (result.ec != std::errc{} || result.ptr != value.data() + value.size() ||
      out < std::numeric_limits<std::int32_t>::min() ||
      out > std::numeric_limits<std::int32_t>::max())
        throw std::runtime_error(std::string("invalid region ") + field);
    return static_cast<std::int32_t>(out);
}

RegionGeometry parse_region(std::string_view text)
{
    std::array<std::string_view, 4> parts{};
    std::size_t start = 0;
    for (std::size_t index = 0; index < parts.size(); ++index)
    {
        const auto comma = text.find(',', start);
        if ((index < parts.size() - 1 && comma == std::string_view::npos) ||
          (index == parts.size() - 1 && comma != std::string_view::npos))
            throw std::runtime_error(
              "invalid region geometry (expected x,y,width,height)");
        const auto end = comma == std::string_view::npos ? text.size() : comma;
        parts[index] = text.substr(start, end - start);
        start = end + 1;
    }

    RegionGeometry region{parse_i32(parts[0], "x"), parse_i32(parts[1], "y"),
      parse_i32(parts[2], "width"), parse_i32(parts[3], "height")};
    if (region.width <= 0 || region.height <= 0)
        throw std::runtime_error("region width and height must be positive");
    return region;
}

ViewportMode parse_viewport(const std::string& value)
{
    if (value == "off") return ViewportMode::off;
    if (value == "on") return ViewportMode::on;
    if (value == "min") return ViewportMode::minimized;
    throw std::runtime_error(
      "invalid viewport mode: " + value + " (expected on, min, or off)");
}

AudioMode parse_audio(const std::string& value)
{
    if (value == "automatic" || value == "auto") return AudioMode::automatic;
    if (value == "app") return AudioMode::app;
    if (value == "system") return AudioMode::system;
    if (value == "off") return AudioMode::off;
    throw std::runtime_error("invalid audio mode: " + value +
      " (expected automatic, app, system, or off)");
}

bool view_selector(const std::optional<protocol::SelectorType>& type)
{
    return type == protocol::SelectorType::focused ||
      type == protocol::SelectorType::app_id ||
      type == protocol::SelectorType::view_id;
}

bool spatial_selector(const std::optional<protocol::SelectorType>& type)
{
    return type == protocol::SelectorType::output ||
      type == protocol::SelectorType::region;
}
}

CliOptions parse_cli(int argc, char** argv)
{
    CliOptions options;
    for (int index = 1; index < argc; ++index)
    {
        const std::string argument = argv[index];
        auto require_value = [&]() -> std::string
        {
            if (++index >= argc)
                throw std::runtime_error("missing value for " + argument);
            return argv[index];
        };

        if (argument == "--")
        {
            for (++index; index < argc; ++index)
                options.launch_command.emplace_back(argv[index]);
            break;
        } else if (argument == "--list") options.list = true;
        else if (argument == "--list-outputs") options.list_outputs = true;
        else if (argument == "--focused")
        {
            if (options.selector_type)
                throw std::runtime_error("only one target selector is allowed");
            options.selector_type = protocol::SelectorType::focused;
        } else if (argument == "--app-id")
        {
            if (options.selector_type)
                throw std::runtime_error("only one target selector is allowed");
            options.selector_type = protocol::SelectorType::app_id;
            options.selector = require_value();
        } else if (argument == "--view-id")
        {
            if (options.selector_type)
                throw std::runtime_error("only one target selector is allowed");
            options.selector_type = protocol::SelectorType::view_id;
            options.view_id = parse_u64(require_value());
        } else if (argument == "--output")
        {
            if (!options.output_name.empty())
                throw std::runtime_error("--output may be specified only once");
            options.output_name = require_value();
        } else if (argument == "--region")
        {
            if (options.selector_type)
                throw std::runtime_error("only one target selector is allowed");
            options.selector_type = protocol::SelectorType::region;
        } else if (argument.starts_with("--region="))
        {
            if (options.selector_type)
                throw std::runtime_error("only one target selector is allowed");
            options.selector_type = protocol::SelectorType::region;
            options.explicit_region = parse_region(
              argument.substr(std::string_view("--region=").size()));
        } else if (argument == "--record")
        {
            if (options.record_disabled)
                throw std::runtime_error("--record cannot be combined with --no-record");
            options.record_path = require_value();
            options.record_explicit = true;
        } else if (argument == "--no-record")
        {
            if (options.record_explicit)
                throw std::runtime_error("--no-record cannot be combined with --record");
            options.record_disabled = true;
        } else if (argument == "--viewport")
        {
            options.viewport = parse_viewport(require_value());
            options.viewport_explicit = true;
        } else if (argument.starts_with("--viewport="))
        {
            options.viewport = parse_viewport(
              argument.substr(std::string_view("--viewport=").size()));
            options.viewport_explicit = true;
        } else if (argument == "--audio")
        {
            options.audio = parse_audio(require_value());
            options.audio_explicit = true;
        } else if (argument.starts_with("--audio="))
        {
            options.audio = parse_audio(
              argument.substr(std::string_view("--audio=").size()));
            options.audio_explicit = true;
        } else if (argument == "--help" || argument == "-h" || argument == "-help")
            options.help = true;
        else if (argument == "--version") options.version = true;
        else throw std::runtime_error("unknown argument: " + argument);
    }

    const bool launching = !options.launch_command.empty();
    if (!options.output_name.empty())
    {
        if (!options.selector_type) options.selector_type = protocol::SelectorType::output;
        else if (*options.selector_type != protocol::SelectorType::region)
            throw std::runtime_error(
              "--output cannot be combined with an application selector");
    }
    if (spatial_selector(options.selector_type)) options.selector = options.output_name;

    const bool capturing = options.selector_type || launching;
    const bool listing = options.list || options.list_outputs;
    if (!options.help && !options.version && !listing && !capturing)
        throw std::runtime_error("a target selector or launch command is required");
    if (options.list && options.list_outputs)
        throw std::runtime_error("choose --list or --list-outputs");
    if (options.selector_type && launching)
        throw std::runtime_error("launch mode cannot be combined with a target selector");
    if (listing && (options.selector_type || options.record_explicit ||
      options.record_disabled || launching || options.viewport_explicit ||
      options.audio_explicit || !options.output_name.empty()))
        throw std::runtime_error("list modes cannot be combined with capture options");
    if (options.selector_type == protocol::SelectorType::output &&
      options.output_name.empty())
        throw std::runtime_error("--output requires an output name");
    if (options.explicit_region && options.output_name.empty())
        throw std::runtime_error("explicit --region=x,y,w,h requires --output NAME");
    return options;
}

void apply_config(CliOptions& options, const FrameTapConfig& config)
{
    const bool launching = !options.launch_command.empty();
    const bool capturing = options.selector_type || launching;
    if (!capturing) return;

    const bool spatial = spatial_selector(options.selector_type);
    if (!options.viewport_explicit)
        options.viewport = spatial ? ViewportMode::off : config.viewport;
    if (!options.audio_explicit)
        options.audio = config.audio_enabled ? config.audio_mode : AudioMode::off;
    if (!options.record_explicit && !options.record_disabled &&
      config.record_by_default)
        options.record_path = next_recording_path(config);

    if (options.audio == AudioMode::app && !launching)
        throw std::runtime_error("--audio=app requires launch-wrapper mode");
    if (options.audio == AudioMode::system && !spatial)
        throw std::runtime_error("--audio=system requires --output or --region");
    if (spatial && options.viewport != ViewportMode::off)
        throw std::runtime_error(
          "output and region capture require --viewport=off to prevent capture recursion");
    if (!options.record_path && options.viewport == ViewportMode::off)
        throw std::runtime_error(
          "capture needs recording enabled or --viewport=on/min");
    if (view_selector(options.selector_type) && options.audio == AudioMode::system)
        throw std::runtime_error("system audio belongs to output/region capture");
}
}
