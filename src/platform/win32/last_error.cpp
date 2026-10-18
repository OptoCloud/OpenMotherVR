// OpenMotherVR-Provenance: clean
#include "platform/win32/last_error.h"

#include <windows.h>

#include <cstdint>

namespace omvr::platform::win32 {

util::Error last_error(std::string_view context) noexcept {
    return {.code = static_cast<std::int32_t>(::GetLastError()), .context = context};
}

} // namespace omvr::platform::win32
