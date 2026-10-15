#include "platform/win32/named_mutex.h"

#include <windows.h>

#include <string>

namespace omvr::platform::win32 {

namespace {

// A mutex its owner ended without releasing still guards a finished build:
// the owner was a process of the same launch that exited.
bool acquired(DWORD wait_result) noexcept {
    return wait_result == WAIT_OBJECT_0 || wait_result == WAIT_ABANDONED;
}

} // namespace

core::ui::ports::LockState NamedMutex::lock(std::wstring_view name,
                                            std::chrono::milliseconds timeout) noexcept {
    try {
        const std::wstring n(name);
        mutex_.reset(::CreateMutexW(nullptr, FALSE, n.c_str()));
    } catch (...) {
        mutex_.reset();
    }
    held_ = mutex_ && acquired(::WaitForSingleObject(mutex_.get(), static_cast<DWORD>(timeout.count())));
    return {held_, static_cast<bool>(mutex_)};
}

void NamedMutex::unlock() noexcept {
    if (held_) ::ReleaseMutex(mutex_.get());
    mutex_.reset();
    held_ = false;
}

} // namespace omvr::platform::win32
