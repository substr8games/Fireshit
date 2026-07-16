#pragma once

#include "frametap/worker/vaapi_encoder.hpp"
#include "frametap/protocol/packet.hpp"

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavfilter/avfilter.h>
#include <libavformat/avformat.h>
#include <libavutil/hwcontext.h>
}

namespace frametap::worker
{
struct VaapiEncoder::Impl
{
    const BufferPool* pool = nullptr;
    AVBufferRef* drm_device = nullptr;
    AVBufferRef* drm_frames = nullptr;
    AVFilterGraph* graph = nullptr;
    AVFilterContext* source = nullptr;
    AVFilterContext* sink = nullptr;
    AVFormatContext* format = nullptr;
    AVCodecContext* codec = nullptr;
    AVStream* stream = nullptr;
    std::filesystem::path final_path;
    std::filesystem::path partial_path;
    bool header_written = false;
    bool committed = false;

    ~Impl();
    void close_resources() noexcept;
    void build_filter(const protocol::SessionOffer& offer,
      const std::filesystem::path& render_node);
    AVFrame* drm_frame(std::uint32_t buffer_id, std::uint64_t pts);
    void sync_surface(AVFrame* frame);
    void ensure_encoder(AVFrame* converted);
    void drain_packets(bool flushing);
    void encode(AVFrame* frame);
};

std::filesystem::path intel_render_node();
void require_av(int result, const char* action);
}
