#include "vaapi_encoder_impl.hpp"
#include "frametap/worker/buffer_pool.hpp"

#include <algorithm>
#include <array>
#include <cerrno>
#include <cstring>
#include <cstdint>
#include <stdexcept>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <drm_fourcc.h>

extern "C" {
#include <libavfilter/buffersink.h>
#include <libavfilter/buffersrc.h>
#include <libavutil/avstring.h>
#include <libavutil/buffer.h>
#include <libavutil/frame.h>
#include <libavutil/hwcontext_drm.h>
#include <libavutil/mem.h>
#include <libavutil/hwcontext_vaapi.h>
#include <va/va.h>
}

namespace frametap::worker
{
namespace
{
AVPixelFormat software_format(std::uint32_t format)
{
    switch (format)
    {
      case DRM_FORMAT_XRGB8888: return AV_PIX_FMT_BGR0;
      case DRM_FORMAT_ARGB8888: return AV_PIX_FMT_BGRA;
      case DRM_FORMAT_XBGR8888: return AV_PIX_FMT_RGB0;
      case DRM_FORMAT_ABGR8888: return AV_PIX_FMT_RGBA;
      default: throw std::runtime_error(
        "Wayfire exported an unsupported RGB DMA-BUF format");
    }
}

int duplicate_fd(int fd)
{
    const auto copy = ::fcntl(fd, F_DUPFD_CLOEXEC, 0);
    if (copy < 0)
    {
        throw std::runtime_error(std::string("could not duplicate DMA-BUF FD: ") +
          std::strerror(errno));
    }
    return copy;
}

std::size_t object_size(int fd, std::uint32_t stride,
  std::uint32_t offset, std::int32_t height)
{
    const auto end = ::lseek(fd, 0, SEEK_END);
    if (end > 0)
    {
        return static_cast<std::size_t>(end);
    }
    return static_cast<std::size_t>(offset) +
      static_cast<std::size_t>(stride) * static_cast<std::size_t>(height);
}

struct ObjectIdentity
{
    dev_t device = 0;
    ino_t inode = 0;
};

ObjectIdentity object_identity(int fd)
{
    struct stat status{};
    if (::fstat(fd, &status) < 0)
    {
        throw std::runtime_error(std::string(
          "could not identify DMA-BUF object: ") + std::strerror(errno));
    }

    return ObjectIdentity{status.st_dev, status.st_ino};
}

bool same_object(const ObjectIdentity& left, const ObjectIdentity& right)
{
    return left.device == right.device && left.inode == right.inode;
}

void free_drm_descriptor(void*, std::uint8_t* data)
{
    auto* descriptor = reinterpret_cast<AVDRMFrameDescriptor*>(data);
    for (int index = 0; index < descriptor->nb_objects; ++index)
    {
        if (descriptor->objects[index].fd >= 0)
        {
            ::close(descriptor->objects[index].fd);
        }
    }
    av_free(descriptor);
}
}

void VaapiEncoder::Impl::build_filter(const protocol::SessionOffer& offer,
  const std::filesystem::path& render_node)
{
    require_av(av_hwdevice_ctx_create(&drm_device, AV_HWDEVICE_TYPE_DRM,
      render_node.c_str(), nullptr, 0), "create DRM device");
    drm_frames = av_hwframe_ctx_alloc(drm_device);
    if (!drm_frames)
    {
        throw std::runtime_error("could not allocate DRM frame context");
    }

    auto* frames = reinterpret_cast<AVHWFramesContext*>(drm_frames->data);
    frames->format = AV_PIX_FMT_DRM_PRIME;
    frames->sw_format = software_format(offer.format);
    frames->width = offer.width;
    frames->height = offer.height;
    require_av(av_hwframe_ctx_init(drm_frames),
      "initialize DRM frame context");

    graph = avfilter_graph_alloc();
    if (!graph)
    {
        throw std::runtime_error("could not allocate VA-API filter graph");
    }
    graph->nb_threads = 1;

    source = avfilter_graph_alloc_filter(graph,
      avfilter_get_by_name("buffer"), "frametap-in");
    if (!source)
    {
        throw std::runtime_error("could not create frame source");
    }
    auto* parameters = av_buffersrc_parameters_alloc();
    if (!parameters)
    {
        throw std::runtime_error("could not allocate frame-source parameters");
    }
    parameters->format = AV_PIX_FMT_DRM_PRIME;
    parameters->width = offer.width;
    parameters->height = offer.height;
    parameters->time_base = AVRational{1, 60};
    parameters->frame_rate = AVRational{60, 1};
    parameters->hw_frames_ctx = av_buffer_ref(drm_frames);
    const int parameter_result = av_buffersrc_parameters_set(source, parameters);
    av_buffer_unref(&parameters->hw_frames_ctx);
    av_free(parameters);
    require_av(parameter_result, "configure frame source");
    require_av(avfilter_init_str(source, nullptr), "initialize frame source");

    require_av(avfilter_graph_create_filter(&sink,
      avfilter_get_by_name("buffersink"), "frametap-out", nullptr, nullptr,
      graph), "create frame sink");

    auto* inputs = avfilter_inout_alloc();
    auto* outputs = avfilter_inout_alloc();
    if (!inputs || !outputs)
    {
        avfilter_inout_free(&inputs);
        avfilter_inout_free(&outputs);
        throw std::runtime_error("could not allocate VA-API filter endpoints");
    }
    outputs->name = av_strdup("in");
    outputs->filter_ctx = source;
    outputs->pad_idx = 0;
    inputs->name = av_strdup("out");
    inputs->filter_ctx = sink;
    inputs->pad_idx = 0;

    const int parse_result = avfilter_graph_parse_ptr(graph,
      "hwmap=derive_device=vaapi,scale_vaapi=format=nv12",
      &inputs, &outputs, nullptr);
    avfilter_inout_free(&inputs);
    avfilter_inout_free(&outputs);
    require_av(parse_result, "create VA-API conversion path");
    require_av(avfilter_graph_config(graph, nullptr),
      "configure VA-API conversion path");
}

AVFrame* VaapiEncoder::Impl::drm_frame(std::uint32_t buffer_id,
  std::uint64_t pts)
{
    const auto& offer = pool->description();
    const auto& record = offer.buffers.at(buffer_id);
    auto* descriptor = static_cast<AVDRMFrameDescriptor*>(
      av_mallocz(sizeof(AVDRMFrameDescriptor)));
    if (!descriptor)
    {
        throw std::runtime_error("could not allocate DRM frame descriptor");
    }

    descriptor->nb_layers = 1;
    descriptor->layers[0].format = offer.format;
    descriptor->layers[0].nb_planes = static_cast<int>(record.plane_count);

    std::array<ObjectIdentity, protocol::max_planes> identities{};
    int object_count = 0;
    try
    {
        for (std::uint32_t plane = 0; plane < record.plane_count; ++plane)
        {
            const int source_fd = pool->fd(buffer_id, plane);
            const auto identity = object_identity(source_fd);
            int object_index = -1;
            for (int candidate = 0; candidate < object_count; ++candidate)
            {
                if (same_object(identity, identities[candidate]))
                {
                    object_index = candidate;
                    break;
                }
            }

            const auto extent = object_size(source_fd,
              record.planes[plane].stride, record.planes[plane].offset,
              offer.height);
            if (object_index < 0)
            {
                object_index = object_count++;
                descriptor->nb_objects = object_count;
                identities[object_index] = identity;
                auto& object = descriptor->objects[object_index];
                object.fd = duplicate_fd(source_fd);
                object.size = extent;
                object.format_modifier = offer.modifier;
            } else
            {
                auto& object = descriptor->objects[object_index];
                object.size = std::max(object.size, extent);
            }

            auto& layer = descriptor->layers[0].planes[plane];
            layer.object_index = object_index;
            layer.offset = record.planes[plane].offset;
            layer.pitch = record.planes[plane].stride;
        }
        descriptor->nb_objects = object_count;
    } catch (...)
    {
        free_drm_descriptor(nullptr,
          reinterpret_cast<std::uint8_t*>(descriptor));
        throw;
    }

    auto* frame = av_frame_alloc();
    if (!frame)
    {
        free_drm_descriptor(nullptr,
          reinterpret_cast<std::uint8_t*>(descriptor));
        throw std::runtime_error("could not allocate input frame");
    }
    frame->buf[0] = av_buffer_create(
      reinterpret_cast<std::uint8_t*>(descriptor), sizeof(*descriptor),
      free_drm_descriptor, nullptr, 0);
    if (!frame->buf[0])
    {
        av_frame_free(&frame);
        free_drm_descriptor(nullptr,
          reinterpret_cast<std::uint8_t*>(descriptor));
        throw std::runtime_error("could not own DRM frame descriptor");
    }
    frame->data[0] = frame->buf[0]->data;
    frame->format = AV_PIX_FMT_DRM_PRIME;
    frame->width = offer.width;
    frame->height = offer.height;
    frame->pts = static_cast<std::int64_t>(pts);
    frame->hw_frames_ctx = av_buffer_ref(drm_frames);
    return frame;
}

void VaapiEncoder::Impl::sync_surface(AVFrame* frame)
{
    auto* frames = reinterpret_cast<AVHWFramesContext*>(frame->hw_frames_ctx->data);
    auto* device = reinterpret_cast<AVHWDeviceContext*>(frames->device_ref->data);
    auto* vaapi = static_cast<AVVAAPIDeviceContext*>(device->hwctx);
    const auto surface = static_cast<VASurfaceID>(
      reinterpret_cast<std::uintptr_t>(frame->data[3]));
    const auto status = vaSyncSurface(vaapi->display, surface);
    if (status != VA_STATUS_SUCCESS)
    {
        throw std::runtime_error(std::string("VA-API conversion failed: ") +
          vaErrorStr(status));
    }
}

void VaapiEncoder::submit(std::uint32_t buffer_id, std::uint64_t pts)
{
    if (!impl->graph || !impl->pool)
    {
        throw std::runtime_error("recording is not open");
    }

    AVFrame* source = impl->drm_frame(buffer_id, pts);
    const int pushed = av_buffersrc_add_frame_flags(impl->source, source,
      AV_BUFFERSRC_FLAG_KEEP_REF);
    av_frame_free(&source);
    require_av(pushed, "import FrameTap DMA-BUF");

    auto* converted = av_frame_alloc();
    if (!converted)
    {
        throw std::runtime_error("could not allocate converted frame");
    }
    const int pulled = av_buffersink_get_frame(impl->sink, converted);
    if (pulled < 0)
    {
        av_frame_free(&converted);
        require_av(pulled, "convert FrameTap frame to NV12");
    }
    impl->sync_surface(converted);
    impl->encode(converted);
    av_frame_free(&converted);
}
}
