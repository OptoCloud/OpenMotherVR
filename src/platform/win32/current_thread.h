// OpenMotherVR-Provenance: clean
#pragma once

#include "core/diag/ports/log_flush.h"
#include "platform/win32/process_info.h"

#include <cstdint>

namespace omvr::platform::win32 {

// The calling thread's id (GetCurrentThreadId), for core logic that tells
// threads apart. Any thread.
class CurrentThread final : public core::diag::ports::CurrentThread {
public:
    std::uint32_t id() const noexcept override { return current_thread_id(); }
};

} // namespace omvr::platform::win32
