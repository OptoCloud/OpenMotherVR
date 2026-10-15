#pragma once

#include <cstddef>
#include <cstdint>

namespace omvr::platform::win32 {

// FILETIME of the Unix epoch (1970-01-01 00:00:00 UTC): 100-ns intervals
// since 1601-01-01.
inline constexpr std::int64_t kFileTimeUnixEpoch = 0x019DB1DED53E8000LL;

// FILETIME ticks (100 ns) per second.
inline constexpr std::int64_t kFileTimeTicksPerSecond = 10'000'000LL;

// MAX_PATH, for buffers declared in headers that do not include windows.h.
inline constexpr std::size_t kMaxPath = 260;

// A path buffer that was too small grows by this factor and is retried.
inline constexpr std::size_t kPathBufferGrowth = 2;

} // namespace omvr::platform::win32
