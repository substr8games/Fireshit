#pragma once

#include <array>
#include <cstdint>
#include "frametap/protocol/packet.hpp"

struct wlr_buffer;

namespace frametap::plugin
{
class LinearBufferPool
{
  public:
    LinearBufferPool() = default;
    LinearBufferPool(const LinearBufferPool&) = delete;
    LinearBufferPool& operator=(const LinearBufferPool&) = delete;
    ~LinearBufferPool();

    void allocate(std::int32_t width, std::int32_t height);
    void release() noexcept;
    wlr_buffer* buffer(std::uint32_t id) const;
    void describe(protocol::SessionOffer& offer) const;
    void append_fds(protocol::Packet& packet) const;
    int export_read_fence(std::uint32_t id) const;

  private:
    std::array<wlr_buffer*, protocol::capture_pool_size> buffers{};
    std::int32_t width = 0;
    std::int32_t height = 0;
};
}
