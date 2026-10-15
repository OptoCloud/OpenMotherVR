#pragma once

#include "util/math/vec.h"

// VR controllers as the game sees them: one frame of semantic controls,
// whichever runtime read them (04 §5.8.1).
namespace omvr::core::input {

using util::Vec2;

// One frame of semantic controls (04 §5.8.1), written to the game's action
// tiers by the ControlsSink. Render thread.
struct Controls {
    Vec2 move;                // left stick / left touchpad (zero when untouched)
    Vec2 turn;                // right stick / right touchpad
    bool turn_engaged = true; // false while a touchpad is not clicked
    float trigger_l = 0, trigger_r = 0;
    bool grip_l = false, grip_r = false;
    bool sprint = false, interact = false, reload = false, radial = false;
    bool map = false, flashlight = false, pause = false;
    bool touchpad_scheme = false; // the touchpad scheme of 04 §5.2 (Vive wands)
};

// One hand's controller as a runtime reports it, before the scheme of
// 04 §5.2 or §5.3 gives its controls their meaning.
struct HandInput {
    Vec2 stick;                // thumbstick or touchpad position
    bool stick_touched = true; // a touchpad is touched (a stick always is)
    bool stick_click   = false;
    float trigger      = 0;
    bool grip          = false;
    bool primary       = false; // lower face button (A / X)
    bool secondary     = false; // upper face button (B / Y)
    bool menu          = false;
};

// The touchpad regions of 04 §5.1: where the pad is pressed, and which
// virtual button a click there is (down interact, up reload, centre radial).
struct TouchpadRegions {
    bool horizontal = false;                               // |x| > |y|
    bool centre     = false;                               // inside the centre zone
    bool interact = false, reload = false, radial = false; // clicked regions
};

// The controller layout in use: picks the button-prompt glyphs (the
// touchpad set or the thumbstick set of 04 §4.1) and the controls help
// shown on the options page (04 §5.7).
enum class ControlScheme { Touchpad, Thumbstick, Index };

} // namespace omvr::core::input
