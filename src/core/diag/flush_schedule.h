// OpenMotherVR-Provenance: clean
#pragma once

#include "core/diag/ports/log_flush.h"
#include "core/diag/thread_roles.h"

#include <chrono>
#include <cstdint>
#include <optional>

namespace omvr::core::diag {

// The mod's flush thread's schedule (22 §14): at most one flush per
// kLogFlushInterval (flush_delay), and the thread recorded as `mod`. Used
// by that thread only, so it needs no synchronisation of its own.
class FlushScheduler final : public ports::FlushSchedule {
public:
    explicit FlushScheduler(ThreadRoles& roles) noexcept : roles_(roles) {}

    void thread_started(std::uint32_t id) noexcept override;
    std::chrono::milliseconds wait_before_flush(TimePoint now) noexcept override;
    void flushed(TimePoint now) noexcept override { last_flush_ = now; }

private:
    ThreadRoles& roles_;
    std::optional<TimePoint> last_flush_;
};

} // namespace omvr::core::diag
