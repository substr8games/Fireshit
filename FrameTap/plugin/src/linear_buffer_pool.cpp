#include "frametap/plugin/linear_buffer_pool.hpp"

#include <cerrno>
#include <cstddef>
#include <fcntl.h>
#include <linux/dma-buf.h>
#include <stdexcept>
#include <system_error>
#include <unistd.h>
#include <sys/ioctl.h>

#include <drm_fourcc.h>

extern "C"
{
#include <wlr/render/allocator.h>
#include <wlr/render/dmabuf.h>
#include <wlr/render/drm_format_set.h>
#include <wlr/types/wlr_buffer.h>
}

#include <wayfire/core.hpp>

namespace frametap::plugin
{
namespace
{
wlr_dmabuf_attributes attributes(wlr_buffer* buffer)
{
    wlr_dmabuf_attributes attrs{};
    if (!buffer || !wlr_buffer_get_dmabuf(buffer, &attrs))
    {
        throw std::runtime_error(
          "linear FrameTap capture buffer is not exportable as DMA-BUF");
    }

    return attrs;
}

int duplicate_fd(int fd)
{
    const int copy = ::fcntl(fd, F_DUPFD_CLOEXEC, 0);
    if (copy < 0)
    {
        throw std::system_error(errno, std::generic_category(),
          "duplicate linear capture DMA-BUF FD");
    }

    return copy;
}
}

LinearBufferPool::~LinearBufferPool()
{
    release();
}

void LinearBufferPool::allocate(std::int32_t new_width, std::int32_t new_height)
{
    if (new_width <= 0 || new_height <= 0)
    {
        throw std::runtime_error("invalid linear capture dimensions");
    }

    if (!wf::get_core().allocator)
    {
        throw std::runtime_error("Wayfire has no active buffer allocator");
    }

    release();
    width = new_width;
    height = new_height;

    wlr_drm_format_set formats{};
    if (!wlr_drm_format_set_add(
          &formats, DRM_FORMAT_XRGB8888, DRM_FORMAT_MOD_LINEAR))
    {
        throw std::runtime_error(
          "could not request linear XRGB8888 allocation format");
    }

    const auto* format =
      wlr_drm_format_set_get(&formats, DRM_FORMAT_XRGB8888);
    if (!format)
    {
        wlr_drm_format_set_finish(&formats);
        throw std::runtime_error(
          "wlroots did not retain the linear XRGB8888 allocation format");
    }

    try
    {
        for (auto& item : buffers)
        {
            item = wlr_allocator_create_buffer(
              wf::get_core().allocator, width, height, format);
            if (!item)
            {
                throw std::runtime_error(
                  "Wayfire allocator cannot create linear XRGB8888 buffers");
            }
        }
    } catch (...)
    {
        wlr_drm_format_set_finish(&formats);
        release();
        throw;
    }

    wlr_drm_format_set_finish(&formats);
}

void LinearBufferPool::release() noexcept
{
    for (auto& item : buffers)
    {
        if (item)
        {
            wlr_buffer_drop(item);
            item = nullptr;
        }
    }

    width = 0;
    height = 0;
}

wlr_buffer* LinearBufferPool::buffer(std::uint32_t id) const
{
    if (id >= buffers.size() || !buffers[id])
    {
        throw std::runtime_error("invalid linear capture buffer ID");
    }

    return buffers[id];
}

void LinearBufferPool::describe(protocol::SessionOffer& offer) const
{
    offer.width = width;
    offer.height = height;
    offer.format = DRM_FORMAT_XRGB8888;
    offer.modifier = DRM_FORMAT_MOD_LINEAR;

    for (std::uint32_t id = 0; id < buffers.size(); ++id)
    {
        const auto attrs = attributes(buffers[id]);
        if (attrs.width != width || attrs.height != height ||
          attrs.format != DRM_FORMAT_XRGB8888 ||
          attrs.modifier != DRM_FORMAT_MOD_LINEAR || attrs.n_planes != 1)
        {
            throw std::runtime_error(
              "allocator did not honor FrameTap's linear XRGB8888 contract");
        }

        auto& record = offer.buffers[id];
        record.buffer_id = id;
        record.plane_count = 1;
        record.planes[0].stride = attrs.stride[0];
        record.planes[0].offset = attrs.offset[0];
    }
}

void LinearBufferPool::append_fds(protocol::Packet& packet) const
{
    const auto first_new_fd = packet.fds.size();
    try
    {
        for (auto* item : buffers)
        {
            const auto attrs = attributes(item);
            packet.fds.push_back(duplicate_fd(attrs.fd[0]));
        }
    } catch (...)
    {
        for (std::size_t index = first_new_fd; index < packet.fds.size(); ++index)
        {
            if (packet.fds[index] >= 0)
            {
                ::close(packet.fds[index]);
            }
        }

        packet.fds.resize(first_new_fd);
        throw;
    }
}

int LinearBufferPool::export_read_fence(std::uint32_t id) const
{
    const auto attrs = attributes(buffer(id));
    dma_buf_export_sync_file request{};
    request.flags = DMA_BUF_SYNC_READ;
    request.fd = -1;
    if (::ioctl(attrs.fd[0], DMA_BUF_IOCTL_EXPORT_SYNC_FILE, &request) < 0)
    {
        throw std::system_error(errno, std::generic_category(),
          "export FrameTap DMA-BUF acquire fence");
    }

    if (request.fd < 0)
    {
        throw std::runtime_error(
          "DMA_BUF_IOCTL_EXPORT_SYNC_FILE returned no acquire fence");
    }

    const int flags = ::fcntl(request.fd, F_GETFD);
    if (flags >= 0)
    {
        ::fcntl(request.fd, F_SETFD, flags | FD_CLOEXEC);
    }

    return request.fd;
}
}
