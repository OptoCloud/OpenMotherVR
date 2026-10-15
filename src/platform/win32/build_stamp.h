#pragma once

#include <cstdint>

namespace omvr::platform::win32 {

// Read *this* module's PE timestamp (i.e. the d3d11.dll proxy itself,
// found in the game's install directory) and convert it to a Windows
// FILETIME (100-ns intervals since 1601-01-01 UTC).
//
// The original packed it as: (PE_timestamp * 10'000'000) + FILETIME_UNIX_EPOCH,
// where FILETIME_UNIX_EPOCH = 0x019DB1DED53E8000.
std::int64_t this_module_build_filetime();

} // namespace omvr::platform::win32
