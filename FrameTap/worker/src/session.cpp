#include "frametap/worker/session.hpp"
#include "frametap/worker/application_audio.hpp"
#include "frametap/worker/buffer_pool.hpp"
#include "frametap/worker/launch.hpp"
#include "frametap/worker/loop_output.hpp"
#include "frametap/worker/plugin_client.hpp"
#include "target_request.hpp"
#include "frametap/worker/vaapi_encoder.hpp"
#include "frametap/protocol/codec.hpp"
#include "frametap/protocol/seqpacket.hpp"

#include <atomic>
#include <cerrno>
#include <csignal>
#include <iostream>
#include <memory>
#include <poll.h>
#include <stdexcept>
#include <unistd.h>
#include <system_error>

namespace frametap::worker
{
namespace
{
std::atomic_bool interrupted = false;
void handle_signal(int) { interrupted.store(true); }

void prepare_recording_directory(
  const std::optional<std::filesystem::path>& recording)
{
    if (!recording || recording->parent_path().empty()) return;
    std::error_code error;
    std::filesystem::create_directories(recording->parent_path(), error);
    if (error)
        throw std::runtime_error("could not create recording directory: " +
          error.message());
}

bool wait_for_acquire_fence(protocol::Packet& packet)
{
    if (packet.fds.size() != 1)
    {
        for (const int fd : packet.fds)
        {
            if (fd >= 0) ::close(fd);
        }
        packet.fds.clear();
        throw std::runtime_error(
          "FrameTap frame did not carry exactly one acquire fence");
    }

    protocol::UniqueFd fence(packet.fds.front());
    packet.fds.clear();
    pollfd descriptor{fence.get(), POLLIN, 0};
    while (!interrupted.load())
    {
        descriptor.revents = 0;
        const int result = ::poll(&descriptor, 1, 250);
        if (result < 0)
        {
            if (errno == EINTR) continue;
            throw std::runtime_error("FrameTap acquire-fence wait failed");
        }
        if (result == 0) continue;
        if (descriptor.revents & POLLIN) return true;
        if (descriptor.revents & (POLLERR | POLLHUP | POLLNVAL))
            throw std::runtime_error("FrameTap acquire fence failed");
    }

    return false;
}

}

int run_session(const CliOptions& options)
{
    interrupted.store(false);
    PluginClient client;
    prepare_recording_directory(options.record_path);

    const auto audio_mode = effective_audio_mode(options);
    std::unique_ptr<ApplicationAudio> audio;
    if (audio_mode == AudioMode::app)
    {
        audio = std::make_unique<ApplicationAudio>(
          AudioCaptureSource::application);
        audio->prepare(options.record_path);
    } else if (audio_mode == AudioMode::system)
    {
        audio = std::make_unique<ApplicationAudio>(AudioCaptureSource::system);
        audio->prepare(options.record_path);
    }

    std::unique_ptr<LaunchedApplication> launched;
    const auto environment = audio ? audio->launch_environment() :
      std::vector<std::pair<std::string, std::string>>{};
    const auto request = resolve_request(
      options, client, launched, environment);
    auto offer_packet = client.prepare(request);
    if (offer_packet.header.type != static_cast<std::uint16_t>(
      protocol::MessageType::session_offer))
    {
        throw std::runtime_error(
          "FrameTap plugin returned an invalid session offer");
    }

    const auto offer = protocol::decode_session_offer(offer_packet);
    const auto session_id = offer_packet.header.session_id;
    BufferPool pool;
    pool.import(offer, std::move(offer_packet.fds));

    VaapiEncoder encoder;
    if (options.record_path)
    {
        const auto& video_path = audio ? audio->video_path() :
          *options.record_path;
        encoder.open(video_path, pool);
        std::cerr << "FrameTap recording: " << options.record_path->string()
                  << " — Intel VA-API H.264 CQP 21";
        if (audio) std::cerr << " + Opus 160 kbps";
        std::cerr << '\n';
    }

    const bool viewport_enabled = options.viewport != ViewportMode::off;
    LoopOutput output;
    if (viewport_enabled)
    {
        const bool minimized = options.viewport == ViewportMode::minimized;
        output.open(pool, [&client, session_id] (
          std::uint32_t id, std::uint64_t sequence)
          { client.release(session_id, id, sequence); }, minimized);
    }

    if (audio)
    {
        audio->start_recording();
        if (audio->source() == AudioCaptureSource::application)
        {
            std::cerr << "FrameTap audio: launched application only — output source ";
        } else
        {
            std::cerr << "FrameTap audio: complete system output mix — monitor source ";
        }
        std::cerr << audio->output_source() << '\n';
    }

    client.start(session_id);
    std::signal(SIGINT, handle_signal);
    std::signal(SIGTERM, handle_signal);
    if (viewport_enabled)
    {
        std::cerr << "FrameTap Output: " << offer.width << 'x' << offer.height
                  << " — " << offer.app_id;
        if (options.viewport == ViewportMode::minimized)
            std::cerr << " — minimized";
        std::cerr << " — press Ctrl+C to stop\n";
    } else
    {
        std::cerr << "FrameTap viewport: off — press Ctrl+C to stop\n";
    }

    while (!interrupted.load() &&
      (!viewport_enabled || !output.close_requested()) &&
      (!launched || launched->alive()))
    {
        if (viewport_enabled)
        {
            output.dispatch_pending();
            output.flush();
        }
        pollfd fds[2]{
          {client.fd(), POLLIN, 0},
          {viewport_enabled ? output.fd() : -1, POLLIN, 0},
        };
        const int result = ::poll(fds, 2, 250);
        if (result < 0)
        {
            if (errno == EINTR) continue;
            throw std::runtime_error("FrameTap event poll failed");
        }
        if (viewport_enabled && (fds[1].revents & POLLIN))
            output.dispatch_readable();
        if (fds[0].revents & POLLIN)
        {
            auto packet = client.receive();
            const auto type = static_cast<protocol::MessageType>(
              packet.header.type);
            if (type == protocol::MessageType::frame_ready)
            {
                const auto frame = protocol::decode_frame_ready(packet);
                if (audio) audio->mark_video_epoch(frame.capture_time_ns);
                if (!wait_for_acquire_fence(packet))
                {
                    client.release(session_id, frame.buffer_id, frame.sequence);
                    continue;
                }
                if (encoder.open())
                    encoder.submit(frame.buffer_id, frame.sequence - 1);
                if (viewport_enabled)
                    output.present(frame.buffer_id, frame.sequence);
                else
                    client.release(session_id, frame.buffer_id, frame.sequence);
            } else if (type == protocol::MessageType::target_closed)
            {
                break;
            } else if (type == protocol::MessageType::error)
            {
                throw std::runtime_error(protocol::decode_error(packet).message);
            }
        }
        if (fds[0].revents & (POLLHUP | POLLERR))
            throw std::runtime_error("FrameTap plugin disconnected");
        if (viewport_enabled && (fds[1].revents & (POLLHUP | POLLERR)))
            break;
    }

    if (interrupted.load() && launched)
    {
        launched->interrupt();
    }
    const auto stopped = client.stop(session_id);
    output.close();
    encoder.finish();
    if (audio) audio->finalize_recording();
    pool.release();
    std::cerr << "FrameTap stopped: " << stopped.frames_sent << " frames, "
              << stopped.frames_dropped << " dropped\n";
    return 0;
}
}
