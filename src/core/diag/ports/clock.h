// OpenMotherVR-Provenance: clean
#pragma once

#include "util/time/steady.h"

#include <cstdint>

namespace omvr::core::diag::ports {

// The local wall-clock time, for the log prefix.
struct LocalTime {
    std::uint16_t year = 0, month = 0, day = 0;
    std::uint16_t hour = 0, minute = 0, second = 0, millisecond = 0;
};

// Time, injected so core logic can be driven by tests. Any thread.
class Clock {
public:
    virtual util::TimePoint now() const noexcept  = 0;
    virtual LocalTime local_time() const noexcept = 0;

protected:
    ~Clock() = default;
};

} // namespace omvr::core::diag::ports
