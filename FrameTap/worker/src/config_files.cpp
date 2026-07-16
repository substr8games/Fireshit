#include "frametap/worker/config.hpp"

#include <chrono>
#include <ctime>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <stdexcept>
#include <string_view>
#include <system_error>
#include <unistd.h>

namespace frametap::worker
{
namespace
{
std::string timestamp_text()
{
    const auto now = std::chrono::system_clock::now();
    const auto value = std::chrono::system_clock::to_time_t(now);
    std::tm local{};
    localtime_r(&value, &local);
    std::ostringstream output;
    output << std::put_time(&local, "%Y%m%d-%H%M%S");
    return output.str();
}

std::string replace_timestamp(std::string pattern)
{
    constexpr std::string_view token = "{timestamp}";
    const auto value = timestamp_text();
    std::size_t position = 0;
    while ((position = pattern.find(token, position)) != std::string::npos)
    {
        pattern.replace(position, token.size(), value);
        position += value.size();
    }
    return pattern;
}

bool occupied(const std::filesystem::path& path)
{
    std::error_code ignored;
    if (std::filesystem::exists(path, ignored)) return true;
    auto partial = path;
    partial += ".part";
    return std::filesystem::exists(partial, ignored);
}

std::filesystem::path collision_safe(std::filesystem::path path)
{
    if (!occupied(path)) return path;
    const auto parent = path.parent_path();
    const auto stem = path.stem().string();
    const auto extension = path.extension().string();
    for (unsigned index = 1; index < 10000; ++index)
    {
        std::ostringstream suffix;
        suffix << '-' << std::setw(2) << std::setfill('0') << index;
        auto candidate = parent / (stem + suffix.str() + extension);
        if (!occupied(candidate)) return candidate;
    }
    throw std::runtime_error("could not allocate a unique FrameTap recording name");
}
}

void write_default_config()
{
    const auto config = compiled_default_config();
    std::error_code error;
    if (std::filesystem::exists(config.file_path, error)) return;
    std::filesystem::create_directories(config.file_path.parent_path(), error);
    if (error)
        throw std::runtime_error("could not create FrameTap config directory: " +
          error.message());

    auto temporary = config.file_path;
    temporary += ".tmp." + std::to_string(::getpid());
    {
        std::ofstream output(temporary, std::ios::trunc);
        if (!output)
            throw std::runtime_error("could not write FrameTap config: " +
              temporary.string());
        output << "# FrameTap user configuration\n"
               << "# CLI options override these values.\n"
               << "# Screenshot fields are reserved for the screenshot pass.\n\n"
               << format_config(config);
        output.flush();
        if (!output)
            throw std::runtime_error("could not finish FrameTap config write");
    }
    std::filesystem::rename(temporary, config.file_path, error);
    if (error)
    {
        std::filesystem::remove(temporary);
        if (!std::filesystem::exists(config.file_path))
            throw std::runtime_error("could not commit FrameTap config: " +
              error.message());
    }
}

void ensure_default_config_exists()
{
    write_default_config();
}

std::filesystem::path next_recording_path(const FrameTapConfig& config)
{
    return collision_safe(config.recordings_directory /
      replace_timestamp(config.recording_name));
}
}
