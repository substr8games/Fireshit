#pragma once

#include <filesystem>
#include <string>

namespace frametap::worker
{
enum class ViewportMode
{
    off,
    on,
    minimized,
};

enum class AudioMode
{
    automatic,
    app,
    system,
    off,
};

struct FrameTapConfig
{
    std::filesystem::path file_path;
    std::filesystem::path recordings_directory;
    std::filesystem::path screenshots_directory;
    std::string recording_name;
    std::string screenshot_name;
    bool record_by_default = true;
    bool audio_enabled = true;
    AudioMode audio_mode = AudioMode::automatic;
    ViewportMode viewport = ViewportMode::off;
    bool loaded_from_file = false;
};

std::filesystem::path user_config_path();
FrameTapConfig compiled_default_config();
FrameTapConfig load_config();
std::string format_config(const FrameTapConfig& config);
void write_default_config();
void ensure_default_config_exists();
std::filesystem::path next_recording_path(const FrameTapConfig& config);
}
