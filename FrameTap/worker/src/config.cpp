#include "frametap/worker/config.hpp"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <fstream>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string_view>

namespace frametap::worker
{
namespace
{
std::string trim(std::string value)
{
    const auto visible = [] (unsigned char c) { return !std::isspace(c); };
    value.erase(value.begin(), std::find_if(value.begin(), value.end(), visible));
    value.erase(std::find_if(value.rbegin(), value.rend(), visible).base(), value.end());
    return value;
}

std::string lower(std::string value)
{
    std::transform(value.begin(), value.end(), value.begin(),
      [] (unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return value;
}

std::filesystem::path home_directory()
{
    const char* home = std::getenv("HOME");
    if (!home || !*home)
        throw std::runtime_error("HOME is not set");
    return home;
}

std::filesystem::path expand_path(const std::string& value)
{
    if (value == "~") return home_directory();
    if (value.starts_with("~/")) return home_directory() / value.substr(2);
    if (value == "$HOME") return home_directory();
    if (value.starts_with("$HOME/")) return home_directory() / value.substr(6);
    const std::filesystem::path path(value);
    return path.is_absolute() ? path : home_directory() / path;
}

bool parse_bool(const std::string& value, std::size_t line)
{
    const auto normalized = lower(value);
    if (normalized == "on" || normalized == "true" || normalized == "yes" || normalized == "1") return true;
    if (normalized == "off" || normalized == "false" || normalized == "no" || normalized == "0") return false;
    throw std::runtime_error("config line " + std::to_string(line) +
      ": expected on/off boolean");
}

AudioMode parse_audio_mode(const std::string& value, std::size_t line)
{
    const auto normalized = lower(value);
    if (normalized == "automatic" || normalized == "auto") return AudioMode::automatic;
    if (normalized == "app") return AudioMode::app;
    if (normalized == "system") return AudioMode::system;
    if (normalized == "off") return AudioMode::off;
    throw std::runtime_error("config line " + std::to_string(line) +
      ": audio_mode must be automatic, app, system, or off");
}

ViewportMode parse_viewport(const std::string& value, std::size_t line)
{
    const auto normalized = lower(value);
    if (normalized == "off") return ViewportMode::off;
    if (normalized == "on") return ViewportMode::on;
    if (normalized == "min") return ViewportMode::minimized;
    throw std::runtime_error("config line " + std::to_string(line) +
      ": viewport must be off, on, or min");
}

void validate_name(const std::string& name, std::string_view field,
  std::string_view extension)
{
    if (name.empty() || name.find('/') != std::string::npos ||
      name.find('\\') != std::string::npos)
        throw std::runtime_error(std::string(field) +
          " must be a filename template without path separators");
    if (!name.ends_with(extension))
        throw std::runtime_error(std::string(field) + " must end with " +
          std::string(extension));
}

std::string compact_path(const std::filesystem::path& path)
{
    const auto home = home_directory();
    const auto relative = path.lexically_relative(home);
    if (!relative.empty() && *relative.begin() != "..")
        return relative == "." ? "~" : "~/" + relative.generic_string();
    return path.generic_string();
}
}

std::filesystem::path user_config_path()
{
    const char* xdg = std::getenv("XDG_CONFIG_HOME");
    if (xdg && *xdg)
        return std::filesystem::path(xdg) / "frametap/config.ini";
    return home_directory() / ".config/frametap/config.ini";
}

FrameTapConfig compiled_default_config()
{
    FrameTapConfig config;
    config.file_path = user_config_path();
    config.recordings_directory = home_directory() / "Captures/recordings";
    config.screenshots_directory = home_directory() / "Captures/screens";
    config.recording_name = "[frametap]-{timestamp}.mkv";
    config.screenshot_name = "[frametap]-{timestamp}.png";
    return config;
}

FrameTapConfig load_config()
{
    auto config = compiled_default_config();
    std::ifstream input(config.file_path);
    if (!input) return config;

    config.loaded_from_file = true;
    std::string section;
    std::string line;
    std::set<std::string> seen;
    std::size_t line_number = 0;
    while (std::getline(input, line))
    {
        ++line_number;
        line = trim(line);
        if (line.empty() || line.starts_with('#') || line.starts_with(';')) continue;
        if (line.front() == '[' && line.back() == ']')
        {
            section = lower(trim(line.substr(1, line.size() - 2)));
            if (section != "paths" && section != "naming" && section != "defaults")
                throw std::runtime_error("config line " + std::to_string(line_number) +
                  ": unknown section [" + section + "]");
            continue;
        }

        const auto equals = line.find('=');
        if (equals == std::string::npos || section.empty())
            throw std::runtime_error("config line " + std::to_string(line_number) +
              ": expected key = value inside a section");
        const auto key = lower(trim(line.substr(0, equals)));
        const auto value = trim(line.substr(equals + 1));
        const auto identity = section + "." + key;
        if (!seen.insert(identity).second)
            throw std::runtime_error("config line " + std::to_string(line_number) +
              ": duplicate key " + identity);

        if (identity == "paths.recordings") config.recordings_directory = expand_path(value);
        else if (identity == "paths.screenshots") config.screenshots_directory = expand_path(value);
        else if (identity == "naming.recording") config.recording_name = value;
        else if (identity == "naming.screenshot") config.screenshot_name = value;
        else if (identity == "defaults.record") config.record_by_default = parse_bool(value, line_number);
        else if (identity == "defaults.audio") config.audio_enabled = parse_bool(value, line_number);
        else if (identity == "defaults.audio_mode") config.audio_mode = parse_audio_mode(value, line_number);
        else if (identity == "defaults.viewport") config.viewport = parse_viewport(value, line_number);
        else throw std::runtime_error("config line " + std::to_string(line_number) +
          ": unknown key " + identity);
    }

    if (config.recordings_directory.empty() || config.screenshots_directory.empty())
        throw std::runtime_error("capture directories may not be empty");
    validate_name(config.recording_name, "naming.recording", ".mkv");
    validate_name(config.screenshot_name, "naming.screenshot", ".png");
    return config;
}

std::string format_config(const FrameTapConfig& config)
{
    const auto audio = config.audio_enabled ? "on" : "off";
    const auto audio_mode = config.audio_mode == AudioMode::automatic ? "automatic" :
      config.audio_mode == AudioMode::app ? "app" :
      config.audio_mode == AudioMode::system ? "system" : "off";
    const auto viewport = config.viewport == ViewportMode::on ? "on" :
      config.viewport == ViewportMode::minimized ? "min" : "off";
    std::ostringstream output;
    output << "[paths]\nrecordings = " << compact_path(config.recordings_directory)
           << "\nscreenshots = " << compact_path(config.screenshots_directory)
           << "\n\n[naming]\nrecording = " << config.recording_name
           << "\nscreenshot = " << config.screenshot_name
           << "\n\n[defaults]\nrecord = " << (config.record_by_default ? "on" : "off")
           << "\naudio = " << audio
           << "\naudio_mode = " << audio_mode
           << "\nviewport = " << viewport << "\n";
    return output.str();
}
}
