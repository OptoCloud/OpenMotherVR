// OpenMotherVR-Provenance: clean
#include "core/vr/space_change.h"

#include <utility>

namespace omvr::core::vr {

void SpaceChange::publish() noexcept {
    if (std::exchange(noted_, false)) pending_.store(true, std::memory_order_release);
}

FrameKindLine FrameKindLog::next(bool head_tracked, bool debug) noexcept {
    if (last_ && *last_ == head_tracked) return FrameKindLine::None;
    last_             = head_tracked;
    const bool always = !tracked_seen_;
    if (head_tracked) tracked_seen_ = true;
    if (always) return FrameKindLine::Always;
    return debug ? FrameKindLine::Debug : FrameKindLine::None;
}

} // namespace omvr::core::vr
