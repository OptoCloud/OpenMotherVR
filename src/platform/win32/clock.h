// OpenMotherVR-Provenance: clean
#pragma once

#include "core/diag/ports/clock.h"

namespace omvr::platform::win32 {

// The system clocks: steady time and the local wall-clock time.
class SystemClock final : public core::diag::ports::Clock {
public:
    std::chrono::steady_clock::time_point now() const noexcept override;
    core::diag::ports::LocalTime local_time() const noexcept override;
};

} // namespace omvr::platform::win32
