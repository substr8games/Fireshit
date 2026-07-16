#pragma once

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <utility>
#include <vector>
#include <sys/types.h>

namespace frametap::worker
{
enum class AudioCaptureSource
{
    application,
    system,
};

class ApplicationAudio
{
  public:
    explicit ApplicationAudio(AudioCaptureSource source);
    ~ApplicationAudio();
    ApplicationAudio(const ApplicationAudio&) = delete;
    ApplicationAudio& operator=(const ApplicationAudio&) = delete;

    void prepare(const std::optional<std::filesystem::path>& recording);
    std::vector<std::pair<std::string, std::string>> launch_environment() const;
    void start_recording();
    void mark_video_epoch(std::uint64_t timestamp_ns) noexcept;
    const std::filesystem::path& video_path() const noexcept;
    void finalize_recording();
    const std::string& output_source() const noexcept;
    AudioCaptureSource source() const noexcept;

  private:
    int load_module(const std::vector<std::string>& arguments);
    void unload_module(int& id) noexcept;
    void stop_recorder() noexcept;
    void cleanup_temporary_files() noexcept;

    AudioCaptureSource capture_source;
    std::string sink_name;
    std::string monitor_name;
    std::string source_name;
    std::string default_sink;
    int sink_module = -1;
    int loopback_module = -1;
    int source_module = -1;
    pid_t recorder = -1;
    std::optional<std::filesystem::path> final_path;
    std::filesystem::path temporary_video;
    std::filesystem::path temporary_audio;
    std::filesystem::path temporary_mux;
    std::uint64_t audio_epoch_ns = 0;
    std::uint64_t video_epoch_ns = 0;
    bool finalized = false;
};
}
