#pragma once

#include "core/diag/ports/clock.h"

#include <chrono>

namespace omvr::core::stereo {

// Head-in-wall blackout delay (not in the original). HeadClipFade
// (VRCameraBehavior) returns the scene visibility: below 1 when the head is
// away from the body or inside geometry. Walking through a door clips the
// head for a moment and flashes the view black. In stereo, a fade that
// starts is held at 1 for 0.6 s and only then let through.
// Analysis: dumps/06c_detours_and_patches.md bdc+3. Camera update, main
// thread only.
class HeadClipDelay {
public:
    explicit HeadClipDelay(const diag::ports::Clock& clock) noexcept : clock_(clock) {}

    // The visibility to hand the game for the game's `fade`.
    float apply(float fade, bool stereo_running) noexcept;

private:
    const diag::ports::Clock& clock_;
    // The last call with the view clear.
    std::chrono::steady_clock::time_point g_head_clear_at{};
};

} // namespace omvr::core::stereo
