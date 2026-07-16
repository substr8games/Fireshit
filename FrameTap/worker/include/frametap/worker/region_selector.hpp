#pragma once

#include <cstdint>
#include <string>

namespace frametap::worker
{
struct SelectedRegion
{
    std::string output;
    std::int32_t x = 0;
    std::int32_t y = 0;
    std::int32_t width = 0;
    std::int32_t height = 0;
};

class RegionSelector
{
  public:
    SelectedRegion select(const std::string& output_name);
};
}
