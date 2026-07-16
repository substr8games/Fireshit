#include "frametap/worker/application_audio.hpp"
#include "command_process.hpp"

#include <cmath>
#include <iomanip>
#include <sstream>
#include <stdexcept>
#include <system_error>
#include <vector>

namespace frametap::worker
{
namespace
{
std::string seconds_text(std::uint64_t nanoseconds)
{
    std::ostringstream output;
    output << std::fixed << std::setprecision(9) <<
      (static_cast<long double>(nanoseconds) / 1'000'000'000.0L);
    return output.str();
}
}

void ApplicationAudio::finalize_recording()
{
    if (!final_path)
    {
        finalized = true;
        return;
    }
    stop_recorder();
    if (!std::filesystem::exists(temporary_video) ||
      !std::filesystem::exists(temporary_audio))
    {
        throw std::runtime_error("FrameTap audio/video intermediate is missing");
    }

    std::vector<std::string> command{"ffmpeg", "-nostdin", "-hide_banner",
      "-loglevel", "error", "-y", "-i", temporary_video.string()};
    std::string audio_filter = "aresample=async=1000:first_pts=0";
    if (audio_epoch_ns && video_epoch_ns && video_epoch_ns > audio_epoch_ns)
    {
        command.insert(command.end(), {"-ss",
          seconds_text(video_epoch_ns - audio_epoch_ns)});
    }
    command.insert(command.end(), {"-i", temporary_audio.string()});
    if (audio_epoch_ns && video_epoch_ns && audio_epoch_ns > video_epoch_ns)
    {
        const auto delay_ms = static_cast<std::uint64_t>(std::llround(
          static_cast<long double>(audio_epoch_ns - video_epoch_ns) /
          1'000'000.0L));
        audio_filter = "adelay=" + std::to_string(delay_ms) +
          ":all=1," + audio_filter;
    }
    command.insert(command.end(), {"-map", "0:v:0", "-map", "1:a:0",
      "-c:v", "copy", "-af", audio_filter, "-c:a", "libopus",
      "-b:a", "160k", "-application", "audio", "-shortest",
      "-avoid_negative_ts", "make_zero", "-f", "matroska",
      temporary_mux.string()});
    command_process::checked(command);

    std::error_code error;
    std::filesystem::rename(temporary_mux, *final_path, error);
    if (error)
    {
        throw std::runtime_error("could not finalize audio/video recording: " +
          error.message());
    }
    std::filesystem::remove(temporary_video, error);
    std::filesystem::remove(temporary_audio, error);
    finalized = true;
}

}
