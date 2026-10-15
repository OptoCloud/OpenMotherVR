// OpenMotherVR-Provenance: clean
#include "platform/win32/proc.h"

#include <windows.h>

namespace omvr::platform::win32 {

void* proc_address(void* module, const char* name) noexcept {
    return reinterpret_cast<void*>(::GetProcAddress(static_cast<HMODULE>(module), name));
}

} // namespace omvr::platform::win32
