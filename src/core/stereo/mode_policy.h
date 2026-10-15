#pragma once

#include "game/layout.h" // R: stereo (A3): decisions over a value struct; the thunks apply them

namespace omvr::core::stereo {

// The stereo mode stack. Mode 0: 3D world with the HUD panel. Mode 1: the
// virtual screen (menus, videos). The stack is kept one deep, so each push
// re-places the screen in front of the current view. Game thread.
// From: FUN_100201e0, FUN_10020220.
class ModePolicy {
public:
    explicit ModePolicy(game::StereoManager& manager) noexcept : manager_(manager) {}

    // Stereo::PushMode(int) and Stereo::PopMode() replaced.
    void push(game::StereoManager* self, int mode) noexcept;
    void pop(game::StereoManager* self) noexcept;

    // Show the virtual screen while a menu is up, and leave it.
    void enter_screen_mode() noexcept;
    void leave_screen_mode() noexcept;

    // Rule f: at the end of each update the stack count equals the mode,
    // after unbalanced game pushes and pops.
    void sync_mode_stack(game::StereoManager& m) noexcept;

private:
    game::StereoManager& manager_;
};

} // namespace omvr::core::stereo
