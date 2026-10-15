// OpenMotherVR-Provenance: clean
#include "platform/win32/unique_handle.h"

#include <windows.h>

#include <utility>

namespace omvr::platform::win32 {

bool UniqueHandle::valid() const noexcept { return handle_ != nullptr && handle_ != INVALID_HANDLE_VALUE; }

void UniqueHandle::reset(void* handle) noexcept {
    if (valid()) ::CloseHandle(handle_);
    handle_ = handle;
}

void UniqueModule::reset(void* module) noexcept {
    void* const old = std::exchange(module_, module);
    if (old) ::FreeLibrary(static_cast<HMODULE>(old));
}

} // namespace omvr::platform::win32
