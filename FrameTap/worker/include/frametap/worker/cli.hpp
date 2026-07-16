#pragma once

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

#include "frametap/protocol/packet.hpp"
#include "frametap/worker/config.hpp"

namespace frametap::worker
{
struct RegionGeometry
{
    std::int32_t x = 0;
    std::int32_t y = 0;
    std::int32_t width = 0;
    std::int32_t height = 0;
};

struct CliOptions
{
    bool list = false;
    bool list_outputs = false;
    bool help = false;
    bool version = false;
    bool audio_explicit = false;
    bool viewport_explicit = false;
    bool record_explicit = false;
    bool record_disabled = false;
    std::optional<protocol::SelectorType> selector_type;
    std::string selector;
    std::string output_name;
    std::uint64_t view_id = 0;
    std::optional<RegionGeometry> explicit_region;
    std::optional<std::filesystem::path> record_path;
    ViewportMode viewport = ViewportMode::off;
    AudioMode audio = AudioMode::automatic;
    std::vector<std::string> launch_command;
};

CliOptions parse_cli(int argc, char** argv);
void apply_config(CliOptions& options, const FrameTapConfig& config);
std::string help_text();
}
