#pragma once

#include "core/diag/ports/log.h"
#include "core/stereo/types.h"
#include "util/sync/once_latch.h"

namespace omvr::core::stereo {

// The stereo mode stack (05 §4.4). Mode 0: 3D world with the HUD panel.
// Mode 1: the virtual screen (menus, videos). The stack is kept one deep, so
// each push re-places the screen in front of the current view. Decisions
// only: the game side applies them. Game thread.
// From: FUN_100201e0, FUN_10020220.
class ModePolicy final {
public:
    explicit ModePolicy(diag::ports::Log& log) noexcept : log_(log) {}

    // Stereo::PushMode(int) and Stereo::PopMode() replaced.
    static ModeSteps push(StereoMode current, StereoMode requested) noexcept;
    static ModeSteps pop(StereoMode current) noexcept;

    // Show the virtual screen while a menu is up, and leave it.
    static ModeSteps enter_screen_mode(StereoMode current) noexcept;
    static ModeSteps leave_screen_mode(StereoMode current) noexcept;

    // Rule f: at the end of each update the stack count equals the mode,
    // after unbalanced game pushes and pops. `can_append`: the game's append
    // routine was found for this build.
    static StackSync sync_mode_stack(const ModeStackState& stack, bool can_append) noexcept;

    // Logs, once each, a sync that could not raise the count.
    void report(StackSyncOutcome outcome) noexcept;

private:
    diag::ports::Log& log_;
    util::OnceLatch   append_failed_logged_;
    util::OnceLatch   no_storage_logged_;
};

} // namespace omvr::core::stereo
