#include "frametap/worker/cli.hpp"

#include <cassert>
#include <filesystem>
#include <stdexcept>

using frametap::worker::AudioMode;
using frametap::worker::FrameTapConfig;
using frametap::worker::ViewportMode;

int main()
{
    FrameTapConfig config;
    config.recordings_directory = "/tmp/frametap-cli-test";
    config.recording_name = "[frametap]-{timestamp}.mkv";
    config.record_by_default = true;
    config.audio_enabled = true;
    config.audio_mode = AudioMode::automatic;
    config.viewport = ViewportMode::off;

    char a0[] = "frametap";
    char a1[] = "--app-id";
    char a2[] = "one-wingedangel";
    char a3[] = "--record";
    char a4[] = "capture.mkv";
    char* selected[] = {a0, a1, a2, a3, a4};
    auto options = frametap::worker::parse_cli(5, selected);
    frametap::worker::apply_config(options, config);
    assert(options.selector_type == frametap::protocol::SelectorType::app_id);
    assert(options.selector == "one-wingedangel");
    assert(options.record_path == std::filesystem::path("capture.mkv"));
    assert(options.audio == AudioMode::automatic);

    char b0[] = "frametap";
    char b1[] = "--record";
    char b2[] = "owa.mkv";
    char b3[] = "--viewport=min";
    char b4[] = "--audio=app";
    char b5[] = "--";
    char b6[] = "./owa.sh";
    char* launched[] = {b0, b1, b2, b3, b4, b5, b6};
    auto wrapper = frametap::worker::parse_cli(7, launched);
    frametap::worker::apply_config(wrapper, config);
    assert(wrapper.viewport == ViewportMode::minimized);
    assert(wrapper.audio == AudioMode::app);
    assert(wrapper.launch_command.size() == 1);

    char c0[] = "frametap";
    char c1[] = "--output";
    char c2[] = "DP-1";
    char* output[] = {c0, c1, c2};
    auto desktop = frametap::worker::parse_cli(3, output);
    frametap::worker::apply_config(desktop, config);
    assert(desktop.selector_type == frametap::protocol::SelectorType::output);
    assert(desktop.output_name == "DP-1");
    assert(desktop.record_path->parent_path() == config.recordings_directory);

    char d0[] = "frametap";
    char d1[] = "--focused";
    char d2[] = "--no-record";
    char d3[] = "--viewport=on";
    char* viewport_only[] = {d0, d1, d2, d3};
    auto no_record = frametap::worker::parse_cli(4, viewport_only);
    frametap::worker::apply_config(no_record, config);
    assert(!no_record.record_path);
    assert(no_record.viewport == ViewportMode::on);

    bool rejected = false;
    try
    {
        char e0[] = "frametap";
        char e1[] = "--output";
        char e2[] = "DP-1";
        char e3[] = "--viewport=on";
        char* recursive[] = {e0, e1, e2, e3};
        auto bad = frametap::worker::parse_cli(4, recursive);
        frametap::worker::apply_config(bad, config);
    } catch (const std::runtime_error&)
    {
        rejected = true;
    }
    assert(rejected);
    return 0;
}
