#pragma once

#include <cstddef>
#include <cstdint>

namespace omvr::core::diag {

// A formatted message is cut to fit this buffer (spec 11: about 1 KB a line).
inline constexpr std::size_t kMessageBufferSize = 1024;
// A whole line, prefix and line ending included.
inline constexpr std::size_t kLineBufferSize = 1280;

// With Dev. Debug Options on, periodic diagnostics (hand offsets, pose age)
// are logged every this many frames.
inline constexpr std::uint32_t kDebugLogPeriodFrames = 900;

// A replacement that throws is logged at most this many times.
inline constexpr int kGuardLogLimit = 16;

} // namespace omvr::core::diag
