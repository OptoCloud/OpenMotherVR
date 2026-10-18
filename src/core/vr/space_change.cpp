// OpenMotherVR-Provenance: clean
#include "core/vr/space_change.h"

#include <utility>

namespace omvr::core::vr {
namespace {

constexpr std::string_view kLogFrameKind       = "[diag] frames now {}";
constexpr std::string_view kLogFrameKindAlways = "{}: frames now {}";

const char* frame_kind_text(bool head_tracked) noexcept { return head_tracked ? "tracked" : "on the screen"; }

} // namespace

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

void FrameKindLog::log(diag::ports::Log& log, bool head_tracked, std::string_view runtime) {
    switch (next(head_tracked, log.debug_enabled())) {
        case FrameKindLine::Always:
            diag::ports::logf(log, kLogFrameKindAlways, runtime, frame_kind_text(head_tracked));
            break;
        case FrameKindLine::Debug:
            diag::ports::logf(log, kLogFrameKind, frame_kind_text(head_tracked));
            break;
        case FrameKindLine::None: break;
    }
}

} // namespace omvr::core::vr
