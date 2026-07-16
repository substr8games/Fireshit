#include "frametap/worker/application_audio.hpp"
#include "command_process.hpp"

#include <cerrno>
#include <charconv>
#include <chrono>
#include <csignal>
#include <stdexcept>
#include <thread>
#include <unistd.h>
#include <sys/wait.h>

namespace frametap::worker
{
namespace
{
std::filesystem::path temporary_base(const std::filesystem::path& final_path)
{
    const auto name = "." + final_path.filename().string() + ".frametap-" +
      std::to_string(::getpid());
    return final_path.parent_path() / name;
}

std::uint64_t monotonic_ns()
{
    return static_cast<std::uint64_t>(std::chrono::duration_cast<std::chrono::nanoseconds>(
      std::chrono::steady_clock::now().time_since_epoch()).count());
}

bool source_exists(const std::string& listing, const std::string& name)
{
    std::size_t start = 0;
    while (start < listing.size())
    {
        const auto end = listing.find('\n', start);
        const auto line = listing.substr(start,
          end == std::string::npos ? std::string::npos : end - start);
        if (line.find('\t' + name + '\t') != std::string::npos ||
          line.starts_with(name + "\t"))
        {
            return true;
        }
        if (end == std::string::npos) break;
        start = end + 1;
    }
    return false;
}
}

ApplicationAudio::ApplicationAudio(AudioCaptureSource source) :
  capture_source(source)
{
    const auto suffix = std::to_string(::getpid());
    sink_name = "frametap_capture_" + suffix;
    monitor_name = sink_name + ".monitor";
    source_name = "frametap_output_" + suffix;
}

ApplicationAudio::~ApplicationAudio()
{
    stop_recorder();
    unload_module(source_module);
    unload_module(loopback_module);
    unload_module(sink_module);
    if (!finalized)
    {
        cleanup_temporary_files();
    }
}

int ApplicationAudio::load_module(const std::vector<std::string>& arguments)
{
    std::vector<std::string> command{"pactl", "load-module"};
    command.insert(command.end(), arguments.begin(), arguments.end());
    const auto output = command_process::checked_output(command);
    int id = -1;
    const auto result = std::from_chars(
      output.data(), output.data() + output.size(), id);
    if (result.ec != std::errc{} || result.ptr != output.data() + output.size())
    {
        throw std::runtime_error("pactl returned an invalid module ID");
    }
    return id;
}

void ApplicationAudio::prepare(
  const std::optional<std::filesystem::path>& recording)
{
    try
    {
        command_process::checked({"pactl", "info"});
        default_sink = command_process::checked_output(
          {"pactl", "get-default-sink"});
        if (default_sink.empty())
        {
            throw std::runtime_error("PipeWire-Pulse has no default output sink");
        }

        if (capture_source == AudioCaptureSource::application)
        {
            sink_module = load_module({"module-null-sink",
              "sink_name=" + sink_name,
              "sink_properties=device.description=FrameTap_Capture",
              "format=s16le", "rate=48000", "channels=2",
              "channel_map=front-left,front-right"});
            loopback_module = load_module({"module-loopback",
              "source=" + monitor_name, "sink=" + default_sink,
              "latency_msec=20", "source_dont_move=true",
              "sink_dont_move=true"});
            source_module = load_module({"module-remap-source",
              "source_name=" + source_name, "master=" + monitor_name,
              "source_properties=device.description=FrameTap_Output",
              "format=s16le", "rate=48000", "channels=2",
              "channel_map=front-left,front-right"});
        } else
        {
            monitor_name = default_sink + ".monitor";
            source_name = monitor_name;
            const auto sources = command_process::checked_output(
              {"pactl", "list", "short", "sources"});
            if (!source_exists(sources, monitor_name))
            {
                throw std::runtime_error(
                  "default output sink has no monitor source: " + monitor_name);
            }
        }

        final_path = recording;
        if (final_path)
        {
            const auto parent = final_path->parent_path();
            if (!parent.empty())
            {
                std::filesystem::create_directories(parent);
            }
            const auto base = temporary_base(*final_path);
            temporary_video = base;
            temporary_video += ".video.mkv";
            temporary_audio = base;
            temporary_audio += ".audio.mka";
            temporary_mux = base;
            temporary_mux += ".mux.part";
            cleanup_temporary_files();
        }
    } catch (...)
    {
        unload_module(source_module);
        unload_module(loopback_module);
        unload_module(sink_module);
        throw;
    }
}

std::vector<std::pair<std::string, std::string>>
ApplicationAudio::launch_environment() const
{
    if (capture_source != AudioCaptureSource::application)
    {
        return {};
    }
    return {{"PULSE_SINK", sink_name}, {"PULSE_LATENCY_MSEC", "20"}};
}

void ApplicationAudio::start_recording()
{
    if (!final_path || recorder > 0)
    {
        return;
    }

    audio_epoch_ns = monotonic_ns();
    recorder = command_process::spawn({"ffmpeg", "-nostdin", "-hide_banner",
      "-loglevel", "error", "-thread_queue_size", "4096", "-f", "pulse",
      "-i", monitor_name, "-ar", "48000", "-ac", "2", "-c:a",
      "flac", "-f", "matroska", temporary_audio.string()});
    std::this_thread::sleep_for(std::chrono::milliseconds(25));
    int status = 0;
    if (::waitpid(recorder, &status, WNOHANG) == recorder)
    {
        recorder = -1;
        throw std::runtime_error("FFmpeg could not open FrameTap audio capture");
    }
}

void ApplicationAudio::mark_video_epoch(std::uint64_t timestamp_ns) noexcept
{
    if (video_epoch_ns == 0)
    {
        video_epoch_ns = timestamp_ns;
    }
}

const std::filesystem::path& ApplicationAudio::video_path() const noexcept
{
    return temporary_video;
}

void ApplicationAudio::stop_recorder() noexcept
{
    if (recorder <= 0)
    {
        return;
    }
    ::kill(-recorder, SIGINT);
    int status = 0;
    for (int attempt = 0; attempt < 50; ++attempt)
    {
        if (::waitpid(recorder, &status, WNOHANG) == recorder)
        {
            recorder = -1;
            return;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(25));
    }
    ::kill(-recorder, SIGTERM);
    for (int attempt = 0; attempt < 10; ++attempt)
    {
        if (::waitpid(recorder, &status, WNOHANG) == recorder)
        {
            recorder = -1;
            return;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(25));
    }
    ::kill(-recorder, SIGKILL);
    while (::waitpid(recorder, &status, 0) < 0 && errno == EINTR)
    {}
    recorder = -1;
}

const std::string& ApplicationAudio::output_source() const noexcept
{
    return source_name;
}

AudioCaptureSource ApplicationAudio::source() const noexcept
{
    return capture_source;
}

void ApplicationAudio::unload_module(int& id) noexcept
{
    if (id < 0)
    {
        return;
    }
    try
    {
        command_process::checked(
          {"pactl", "unload-module", std::to_string(id)});
    } catch (...)
    {}
    id = -1;
}

void ApplicationAudio::cleanup_temporary_files() noexcept
{
    std::error_code ignored;
    if (!temporary_video.empty()) std::filesystem::remove(temporary_video, ignored);
    if (!temporary_audio.empty()) std::filesystem::remove(temporary_audio, ignored);
    if (!temporary_mux.empty()) std::filesystem::remove(temporary_mux, ignored);
}
}
