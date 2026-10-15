#include "platform/win32/named_mutex.h"

#include <windows.h>

#include <string>

namespace omvr::platform::win32 {

core::ui::ports::LockState NamedMutex::lock(std::wstring_view name, std::chrono::milliseconds timeout) noexcept {
    try {
        const std::wstring n(name);
        mutex_ = ::CreateMutexW(nullptr, FALSE, n.c_str());
    } catch (...) {
        mutex_ = nullptr;
    }
    held_ = false;
    if (mutex_) {
        const DWORD w = ::WaitForSingleObject(mutex_, static_cast<DWORD>(timeout.count()));
        held_ = w == WAIT_OBJECT_0 || w == WAIT_ABANDONED;
    }
    return {held_, mutex_ != nullptr};
}

void NamedMutex::unlock() noexcept {
    if (held_) ::ReleaseMutex(mutex_);
    if (mutex_) ::CloseHandle(mutex_);
    mutex_ = nullptr;
    held_  = false;
}

} // namespace omvr::platform::win32
