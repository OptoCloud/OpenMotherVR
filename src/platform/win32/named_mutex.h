#pragma once

#include "core/ui/ports/archive_files.h"

#include <chrono>
#include <string_view>

namespace omvr::platform::win32 {

// A named mutex shared between processes, taken with a timeout. Not thread
// safe; one lock at a time.
class NamedMutex {
public:
    // Creates or opens `name` and waits up to `timeout` for it. An abandoned
    // mutex counts as held.
    core::ui::ports::LockState lock(std::wstring_view name, std::chrono::milliseconds timeout) noexcept;
    // Releases what lock() took and closes the handle.
    void unlock() noexcept;

private:
    void* mutex_ = nullptr; // HANDLE
    bool  held_  = false;
};

} // namespace omvr::platform::win32
