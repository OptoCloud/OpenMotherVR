#pragma once

#include "util/math/vec.h"

// VR controllers as the game sees them: one frame of semantic controls,
// whichever runtime read them.
namespace omvr::core::input {

using util::Vec2;

struct Controls {
    Vec2  move;                 // left stick / left touchpad (zero when untouched)
    Vec2  turn;                 // right stick / right touchpad
    bool  turn_engaged = true;  // false while a touchpad is not clicked
    float trigger_l = 0, trigger_r = 0;
    bool  grip_l = false, grip_r = false;
    bool  sprint = false, interact = false, reload = false, radial = false;
    bool  map = false, flashlight = false, pause = false;
    bool  touchpad_scheme = false;  // Vive wand: the original's touchpad behaviour
};

// The original's touchpad regions: where the pad is pressed, and which
// virtual button a click there is (down interact, up reload, centre radial).
// From: OpenVR PollInput, FUN_10007ad0.
struct TouchpadRegions {
    bool horizontal = false;   // |x| > |y|
    bool centre     = false;   // inside the dead zone
    bool interact = false, reload = false, radial = false;   // clicked regions
};

// The controller layout in use: picks the button-prompt glyphs (the
// original's Vive wand set or its Oculus Touch set) and the controls help
// shown on the MotherVR options page.
enum class ControlScheme { Touchpad, Thumbstick, Index };

} // namespace omvr::core::input
