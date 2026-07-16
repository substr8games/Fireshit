#pragma once

#include <cstdint>
#include <functional>
#include <memory>
#include "frametap/worker/buffer_pool.hpp"

namespace frametap::worker
{
class LoopOutput
{
  public:
    using Release = std::function<void(std::uint32_t, std::uint64_t)>;
    LoopOutput();
    ~LoopOutput();
    LoopOutput(const LoopOutput&) = delete;
    LoopOutput& operator=(const LoopOutput&) = delete;

    void open(const BufferPool& pool, Release release, bool minimized);
    void present(std::uint32_t buffer_id, std::uint64_t sequence);
    void dispatch_pending();
    void dispatch_readable();
    void flush();
    void close() noexcept;
    int fd() const noexcept;
    bool ready() const noexcept;
    bool close_requested() const noexcept;
  private:
    struct Impl;
    std::unique_ptr<Impl> impl;
};
}
