#pragma once

#include <cstdint>
#include <filesystem>
#include <memory>

namespace frametap::worker
{
class BufferPool;

class VaapiEncoder
{
  public:
    VaapiEncoder();
    ~VaapiEncoder();
    VaapiEncoder(const VaapiEncoder&) = delete;
    VaapiEncoder& operator=(const VaapiEncoder&) = delete;

    bool available() const;
    void open(const std::filesystem::path& path, const BufferPool& pool);
    void submit(std::uint32_t buffer_id, std::uint64_t pts);
    void finish();
    bool open() const noexcept;

  private:
    struct Impl;
    std::unique_ptr<Impl> impl;
};
}
