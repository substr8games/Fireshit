#include "frametap/worker/buffer_pool.hpp"
#include <stdexcept>
#include <unistd.h>

namespace frametap::worker
{
void BufferPool::import(const protocol::SessionOffer& new_offer, std::vector<int> fds)
{
    release(); offer = new_offer; first_plane.resize(protocol::capture_pool_size);
    std::size_t expected = 0;
    for (const auto& buffer : offer.buffers)
    {
        if (buffer.buffer_id >= protocol::capture_pool_size) throw std::runtime_error("invalid capture buffer ID");
        first_plane[buffer.buffer_id] = expected; expected += buffer.plane_count;
    }
    if (fds.size() != expected)
    {
        for (int fd : fds) if (fd >= 0) ::close(fd);
        throw std::runtime_error("DMA-BUF descriptor count does not match session offer");
    }
    plane_fds.reserve(fds.size());
    for (int fd : fds) plane_fds.emplace_back(fd);
}

void BufferPool::release() noexcept
{
    plane_fds.clear(); first_plane.clear(); offer = {};
}

int BufferPool::fd(std::uint32_t buffer_id, std::uint32_t plane) const
{
    if (buffer_id >= first_plane.size()) throw std::runtime_error("invalid capture buffer ID");
    const auto& record = offer.buffers[buffer_id];
    if (plane >= record.plane_count) throw std::runtime_error("invalid DMA-BUF plane");
    return plane_fds.at(first_plane[buffer_id] + plane).get();
}

const protocol::SessionOffer& BufferPool::description() const noexcept { return offer; }
}
