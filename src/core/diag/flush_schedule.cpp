// OpenMotherVR-Provenance: clean
#include "core/diag/flush_schedule.h"

#include "core/diag/flush_policy.h"

namespace omvr::core::diag {

void FlushScheduler::thread_started(std::uint32_t id) noexcept { roles_.set(ThreadRole::Mod, id); }

std::chrono::milliseconds FlushScheduler::wait_before_flush(TimePoint now) noexcept {
    return flush_delay(now, last_flush_);
}

} // namespace omvr::core::diag
