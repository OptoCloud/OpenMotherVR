#pragma once

#include <cstdint>

namespace omvr::platform::win32 {

// This module's PE timestamp (the d3d11.dll proxy itself, in the game's
// install directory) as a Windows FILETIME (100-ns intervals since
// 1601-01-01 UTC): seconds * kFileTimeTicksPerSecond + kFileTimeUnixEpoch.
// 0 if the headers cannot be read. The payload files are stamped with it,
// so a mod update replaces them exactly once.
std::int64_t this_module_build_filetime() noexcept;

} // namespace omvr::platform::win32
