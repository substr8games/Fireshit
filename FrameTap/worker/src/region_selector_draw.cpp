#include "region_selector_impl.hpp"

#include <algorithm>
#include <fcntl.h>
#include <stdexcept>
#include <string>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

namespace frametap::worker::detail
{
int SelectorClient::create_shm(std::size_t bytes)
{
    static unsigned counter = 0;
    const std::string name = "/frametap-selector-" +
      std::to_string(::getpid()) + "-" + std::to_string(++counter);
    const int fd = ::shm_open(name.c_str(), O_RDWR | O_CREAT | O_EXCL, 0600);
    if (fd < 0)
    {
        throw std::runtime_error("could not create selector shared memory");
    }
    ::shm_unlink(name.c_str());
    if (::ftruncate(fd, static_cast<off_t>(bytes)) < 0)
    {
        ::close(fd);
        throw std::runtime_error("could not size selector shared memory");
    }
    return fd;
}

void SelectorClient::allocate_buffers()
{
    const std::size_t stride = static_cast<std::size_t>(surface_width) * 4U;
    const std::size_t bytes = stride * static_cast<std::size_t>(surface_height);
    for (auto& buffer : buffers)
    {
        const int fd = create_shm(bytes);
        buffer.owner = this;
        buffer.bytes = bytes;
        buffer.mapping = ::mmap(nullptr, bytes, PROT_READ | PROT_WRITE,
          MAP_SHARED, fd, 0);
        if (buffer.mapping == MAP_FAILED)
        {
            ::close(fd);
            throw std::runtime_error("could not map selector shared memory");
        }

        auto* pool = wl_shm_create_pool(shm, fd, static_cast<std::int32_t>(bytes));
        buffer.handle = wl_shm_pool_create_buffer(pool, 0,
          surface_width, surface_height, static_cast<std::int32_t>(stride),
          WL_SHM_FORMAT_ARGB8888);
        wl_shm_pool_destroy(pool);
        ::close(fd);
        if (!buffer.handle)
        {
            throw std::runtime_error("could not create selector Wayland buffer");
        }
        wl_buffer_add_listener(buffer.handle, &buffer_listener, &buffer);
    }
}

void SelectorClient::redraw()
{
    Buffer* buffer = nullptr;
    for (auto& candidate : buffers)
    {
        if (!candidate.busy)
        {
            buffer = &candidate;
            break;
        }
    }
    if (!buffer)
    {
        redraw_pending = true;
        return;
    }
    redraw_pending = false;

    auto* pixels = static_cast<std::uint32_t*>(buffer->mapping);
    std::fill(pixels, pixels +
      static_cast<std::size_t>(surface_width) * surface_height, 0xaa000000U);

    auto x = anchor_x;
    auto y = anchor_y;
    auto width = pointer_x - anchor_x;
    auto height = pointer_y - anchor_y;
    if (dragging)
    {
        if (width < 0) { x += width; width = -width; }
        if (height < 0) { y += height; height = -height; }
        x = std::clamp(x, 0, surface_width);
        y = std::clamp(y, 0, surface_height);
        width = std::clamp(width, 0, surface_width - x);
        height = std::clamp(height, 0, surface_height - y);
        for (std::int32_t row = y; row < y + height; ++row)
        {
            auto* line = pixels + static_cast<std::size_t>(row) * surface_width;
            std::fill(line + x, line + x + width, 0x00000000U);
        }
        draw_border(pixels, x, y, width, height);
    }
    draw_crosshair(pixels);

    buffer->busy = true;
    wl_surface_attach(surface, buffer->handle, 0, 0);
    wl_surface_damage_buffer(surface, 0, 0, surface_width, surface_height);
    wl_surface_commit(surface);
    wl_display_flush(display);
}

void SelectorClient::draw_border(std::uint32_t* pixels, std::int32_t x,
  std::int32_t y, std::int32_t width, std::int32_t height)
{
    if (width <= 0 || height <= 0)
    {
        return;
    }
    for (std::int32_t thickness = 0; thickness < 2; ++thickness)
    {
        const auto left = std::clamp(x + thickness, 0, surface_width - 1);
        const auto right = std::clamp(x + width - 1 - thickness, 0,
          surface_width - 1);
        const auto top = std::clamp(y + thickness, 0, surface_height - 1);
        const auto bottom = std::clamp(y + height - 1 - thickness, 0,
          surface_height - 1);
        for (std::int32_t column = left; column <= right; ++column)
        {
            pixels[static_cast<std::size_t>(top) * surface_width + column] =
              0xffffffffU;
            pixels[static_cast<std::size_t>(bottom) * surface_width + column] =
              0xffffffffU;
        }
        for (std::int32_t row = top; row <= bottom; ++row)
        {
            pixels[static_cast<std::size_t>(row) * surface_width + left] =
              0xffffffffU;
            pixels[static_cast<std::size_t>(row) * surface_width + right] =
              0xffffffffU;
        }
    }
}

void SelectorClient::draw_crosshair(std::uint32_t* pixels)
{
    const auto x = std::clamp(pointer_x, 0, surface_width - 1);
    const auto y = std::clamp(pointer_y, 0, surface_height - 1);
    for (std::int32_t offset = -10; offset <= 10; ++offset)
    {
        const auto column = x + offset;
        const auto row = y + offset;
        if (column >= 0 && column < surface_width)
            pixels[static_cast<std::size_t>(y) * surface_width + column] =
              0xffffffffU;
        if (row >= 0 && row < surface_height)
            pixels[static_cast<std::size_t>(row) * surface_width + x] =
              0xffffffffU;
    }
}

void SelectorClient::finish_selection()
{
    auto x = anchor_x;
    auto y = anchor_y;
    auto width = pointer_x - anchor_x;
    auto height = pointer_y - anchor_y;
    if (width < 0) { x += width; width = -width; }
    if (height < 0) { y += height; height = -height; }
    x = std::clamp(x, 0, surface_width);
    y = std::clamp(y, 0, surface_height);
    width = std::clamp(width, 0, surface_width - x);
    height = std::clamp(height, 0, surface_height - y);
    if (width < 2 || height < 2)
    {
        dragging = false;
        redraw();
        return;
    }

    selected_x = x;
    selected_y = y;
    selected_width = width;
    selected_height = height;
    finished = true;
}
}
