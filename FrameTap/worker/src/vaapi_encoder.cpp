#include "vaapi_encoder_impl.hpp"
#include "frametap/worker/buffer_pool.hpp"

#include <fstream>
#include <stdexcept>
#include <string>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/error.h>
#include <libavutil/hwcontext.h>
}

namespace frametap::worker
{
namespace
{
std::string av_error(int code)
{
    char text[AV_ERROR_MAX_STRING_SIZE]{};
    av_strerror(code, text, sizeof(text));
    return text;
}
}

void require_av(int result, const char* action)
{
    if (result < 0)
    {
        throw std::runtime_error(std::string(action) + ": " + av_error(result));
    }
}

std::filesystem::path intel_render_node()
{
    const std::filesystem::path sys("/sys/class/drm");
    std::error_code ignored;
    for (const auto& entry : std::filesystem::directory_iterator(sys, ignored))
    {
        const auto name = entry.path().filename().string();
        if (!name.starts_with("renderD"))
        {
            continue;
        }

        std::ifstream vendor(entry.path() / "device/vendor");
        std::string value;
        vendor >> value;
        if (value == "0x8086")
        {
            const auto node = std::filesystem::path("/dev/dri") / name;
            if (std::filesystem::exists(node))
            {
                return node;
            }
        }
    }

    throw std::runtime_error("no Intel DRM render node is available");
}

VaapiEncoder::Impl::~Impl()
{
    close_resources();
    if (!committed && !partial_path.empty())
    {
        std::error_code ignored;
        std::filesystem::remove(partial_path, ignored);
    }
}

void VaapiEncoder::Impl::close_resources() noexcept
{
    avcodec_free_context(&codec);
    if (format)
    {
        if (format->pb)
        {
            avio_closep(&format->pb);
        }
        avformat_free_context(format);
        format = nullptr;
    }
    avfilter_graph_free(&graph);
    av_buffer_unref(&drm_frames);
    av_buffer_unref(&drm_device);
    source = nullptr;
    sink = nullptr;
    stream = nullptr;
}

VaapiEncoder::VaapiEncoder() : impl(std::make_unique<Impl>()) {}
VaapiEncoder::~VaapiEncoder() = default;

bool VaapiEncoder::available() const
{
    try
    {
        return avcodec_find_encoder_by_name("h264_vaapi") &&
          !intel_render_node().empty();
    } catch (...)
    {
        return false;
    }
}

void VaapiEncoder::open(const std::filesystem::path& path,
  const BufferPool& pool)
{
    if (impl->format)
    {
        throw std::runtime_error("recording is already open");
    }
    if (!available())
    {
        throw std::runtime_error("Intel VA-API H.264 is unavailable");
    }

    impl->pool = &pool;
    impl->final_path = path;
    impl->partial_path = path;
    impl->partial_path += ".part";
    std::error_code ignored;
    std::filesystem::remove(impl->partial_path, ignored);
    require_av(avformat_alloc_output_context2(&impl->format, nullptr,
      "matroska", impl->partial_path.c_str()), "create Matroska output");
    impl->build_filter(pool.description(), intel_render_node());
}

bool VaapiEncoder::open() const noexcept
{
    return impl->format != nullptr;
}
}
