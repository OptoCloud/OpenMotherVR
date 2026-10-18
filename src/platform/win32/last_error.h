// OpenMotherVR-Provenance: clean
#pragma once

#include "util/base/error.h"

#include <string_view>

namespace omvr::platform::win32 {

// GetLastError() as an error, with what was being done: call it right after
// the failing call, before anything else can change the thread's error.
util::Error last_error(std::string_view context) noexcept;

} // namespace omvr::platform::win32
