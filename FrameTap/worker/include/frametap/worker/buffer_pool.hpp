#pragma once

#include <cstdint>
#include <vector>
#include "frametap/protocol/packet.hpp"
#include "frametap/protocol/seqpacket.hpp"

namespace frametap::worker
{
class BufferPool
{
  public:
    void import(const protocol::SessionOffer& offer, std::vector<int> fds);
    void release() noexcept;
    int fd(std::uint32_t buffer_id, std::uint32_t plane) const;
    const protocol::SessionOffer& description() const noexcept;
  private:
    protocol::SessionOffer offer;
    std::vector<protocol::UniqueFd> plane_fds;
    std::vector<std::size_t> first_plane;
};
}
