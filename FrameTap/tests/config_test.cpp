#include "frametap/worker/config.hpp"

#include <cassert>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <stdexcept>

int main()
{
    const std::filesystem::path root = "/tmp/frametap-config-test";
    std::filesystem::remove_all(root);
    std::filesystem::create_directories(root);
    setenv("HOME", (root / "home").c_str(), 1);
    setenv("XDG_CONFIG_HOME", (root / "xdg").c_str(), 1);

    const auto expected = root / "xdg/frametap/config.ini";
    assert(frametap::worker::user_config_path() == expected);
    auto defaults = frametap::worker::load_config();
    assert(!defaults.loaded_from_file);
    assert(defaults.record_by_default);
    assert(defaults.audio_enabled);
    assert(defaults.recording_name == "[frametap]-{timestamp}.mkv");

    frametap::worker::write_default_config();
    assert(std::filesystem::exists(expected));
    auto loaded = frametap::worker::load_config();
    assert(loaded.loaded_from_file);
    assert(loaded.recordings_directory == root / "home/Captures/recordings");

    {
        std::ofstream output(expected, std::ios::trunc);
        output << "[paths]\nrecordings = ~/Video\nscreenshots = ~/Pictures\n\n"
               << "[naming]\nrecording = capture-{timestamp}.mkv\n"
               << "screenshot = shot-{timestamp}.png\n\n"
               << "[defaults]\nrecord = off\naudio = off\n"
               << "audio_mode = automatic\nviewport = min\n";
    }
    loaded = frametap::worker::load_config();
    assert(!loaded.record_by_default);
    assert(!loaded.audio_enabled);
    assert(loaded.viewport == frametap::worker::ViewportMode::minimized);
    assert(frametap::worker::format_config(loaded).find("record = off") !=
      std::string::npos);

    {
        std::ofstream output(expected, std::ios::trunc);
        output << "[defaults]\nbanana = on\n";
    }
    bool rejected = false;
    try
    {
        static_cast<void>(frametap::worker::load_config());
    } catch (const std::runtime_error&)
    {
        rejected = true;
    }
    assert(rejected);
    std::filesystem::remove_all(root);
    return 0;
}
