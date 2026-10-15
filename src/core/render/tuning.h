#pragma once

#include <cstdint>

namespace omvr::core::render {

// The dither fix reads the target size from cb2[39] = (W, H, 1/W, 1/H), the
// register the game fills with the render target's resolution.
inline constexpr std::uint32_t kSourceResolution = 39;

// The pixel-shader statistics are logged on the first fixed shader and then
// every this many.
inline constexpr int kShaderStatsLogPeriod = 200;

// A shader the driver refuses after the fix is logged this many times.
inline constexpr int kShaderRejectLogLimit = 3;

} // namespace omvr::core::render
