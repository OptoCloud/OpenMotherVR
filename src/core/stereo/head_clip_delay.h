#pragma once

#include "core/diag/ports/clock.h"

#include <chrono>

namespace omvr::core::stereo {

// Head-in-wall blackout delay (08 §4). HeadClipFade (VRCameraBehavior)
// returns the scene visibility: below 1 when the head is away from the body
// or inside geometry. Walking through a door clips the head for a moment
// and flashes the view black. In stereo, a fade that starts is held at 1
// for kFadeGrace and only then let through. Camera update, game thread.
class HeadClipDelay final {
public:
    explicit HeadClipDelay(const diag::ports::Clock& clock) noexcept : clock_(clock) {}

    // The visibility to hand the game for the game's `fade`, while the
    // stereo device runs or not.
    float apply(float fade, bool stereo_running) noexcept;

private:
    const diag::ports::Clock& clock_;
    // The last call with the view clear.
    std::chrono::steady_clock::time_point clear_at_{};
};

} // namespace omvr::core::stereo
