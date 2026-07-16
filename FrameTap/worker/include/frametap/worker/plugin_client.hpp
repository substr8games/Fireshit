#pragma once

#include <vector>
#include "frametap/protocol/seqpacket.hpp"
#include "frametap/protocol/packet.hpp"

namespace frametap::worker
{
class PluginClient
{
  public:
    PluginClient();
    std::vector<protocol::ViewRecord> list_views();
    std::vector<protocol::OutputRecord> list_outputs();
    protocol::Packet prepare(const protocol::PrepareSession& request);
    void start(std::uint64_t session_id);
    protocol::SessionStopped stop(std::uint64_t session_id);
    void release(std::uint64_t session_id, std::uint32_t buffer_id, std::uint64_t sequence);
    protocol::Packet receive();
    int fd() const noexcept;
  private:
    protocol::Packet transact(protocol::Packet request);
    protocol::UniqueFd socket;
    std::uint64_t next_request_id = 1;
};
}
