#include "vaapi_encoder_impl.hpp"

#include <cerrno>
#include <stdexcept>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/error.h>
#include <libavutil/frame.h>
#include <libavutil/hwcontext.h>
#include <libavutil/opt.h>
}

namespace frametap::worker
{
void VaapiEncoder::Impl::ensure_encoder(AVFrame* converted)
{
    if (codec)
    {
        return;
    }

    const auto* encoder = avcodec_find_encoder_by_name("h264_vaapi");
    if (!encoder)
    {
        throw std::runtime_error("FFmpeg has no h264_vaapi encoder");
    }

    auto configure = [&] (bool low_power)
    {
        codec = avcodec_alloc_context3(encoder);
        if (!codec)
        {
            throw std::runtime_error("could not allocate H.264 encoder");
        }
        codec->width = converted->width;
        codec->height = converted->height;
        codec->time_base = AVRational{1, 60};
        codec->framerate = AVRational{60, 1};
        codec->pix_fmt = AV_PIX_FMT_VAAPI;
        codec->gop_size = 120;
        codec->max_b_frames = 0;
        codec->hw_frames_ctx = av_buffer_ref(converted->hw_frames_ctx);
        if (format->oformat->flags & AVFMT_GLOBALHEADER)
        {
            codec->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
        }
        av_opt_set(codec, "rc_mode", "CQP", AV_OPT_SEARCH_CHILDREN);
        av_opt_set_int(codec, "qp", 21, AV_OPT_SEARCH_CHILDREN);
        av_opt_set_int(codec, "async_depth", 1, AV_OPT_SEARCH_CHILDREN);
        av_opt_set_int(codec, "low_power", low_power ? 1 : 0,
          AV_OPT_SEARCH_CHILDREN);
        return avcodec_open2(codec, encoder, nullptr);
    };

    int result = configure(true);
    if (result < 0)
    {
        avcodec_free_context(&codec);
        result = configure(false);
    }
    require_av(result, "open Intel VA-API H.264 encoder");

    stream = avformat_new_stream(format, nullptr);
    if (!stream)
    {
        throw std::runtime_error("could not create Matroska video stream");
    }
    stream->time_base = codec->time_base;
    stream->avg_frame_rate = codec->framerate;
    stream->r_frame_rate = codec->framerate;
    require_av(avcodec_parameters_from_context(stream->codecpar, codec),
      "copy H.264 stream parameters");
    require_av(avio_open(&format->pb, partial_path.c_str(), AVIO_FLAG_WRITE),
      "open recording file");
    require_av(avformat_write_header(format, nullptr),
      "write Matroska header");
    header_written = true;
}

void VaapiEncoder::Impl::drain_packets(bool flushing)
{
    auto* packet = av_packet_alloc();
    if (!packet)
    {
        throw std::runtime_error("could not allocate H.264 packet");
    }
    for (;;)
    {
        const int result = avcodec_receive_packet(codec, packet);
        if (result == AVERROR(EAGAIN) || result == AVERROR_EOF)
        {
            av_packet_free(&packet);
            return;
        }
        if (result < 0)
        {
            av_packet_free(&packet);
            require_av(result, flushing ? "flush H.264 encoder" :
              "receive H.264 packet");
        }
        av_packet_rescale_ts(packet, codec->time_base, stream->time_base);
        packet->stream_index = stream->index;
        const int written = av_interleaved_write_frame(format, packet);
        av_packet_unref(packet);
        if (written < 0)
        {
            av_packet_free(&packet);
            require_av(written, "write H.264 packet");
        }
    }
}

void VaapiEncoder::Impl::encode(AVFrame* frame)
{
    ensure_encoder(frame);
    require_av(avcodec_send_frame(codec, frame), "submit H.264 frame");
    drain_packets(false);
}

void VaapiEncoder::finish()
{
    if (!impl->format)
    {
        return;
    }
    if (!impl->codec)
    {
        throw std::runtime_error("recording ended before the first frame");
    }

    require_av(avcodec_send_frame(impl->codec, nullptr), "begin H.264 flush");
    impl->drain_packets(true);
    if (impl->header_written)
    {
        require_av(av_write_trailer(impl->format), "write Matroska trailer");
    }
    if (impl->format->pb)
    {
        require_av(avio_closep(&impl->format->pb), "close recording file");
    }

    std::error_code error;
    std::filesystem::rename(impl->partial_path, impl->final_path, error);
    if (error)
    {
        throw std::runtime_error("could not finalize recording: " +
          error.message());
    }
    impl->committed = true;
    impl->close_resources();
}
}
