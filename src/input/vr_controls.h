#pragma once

// VR controllers as the game sees them: one frame of semantic controls,
// whichever runtime read them, written into the gamepad action tiers.
#include <span>

namespace mothervr::input {

struct Vec2 {
    float x = 0, y = 0;
};

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

// A touchpad reading (Vive wands) inside this square counts as its centre.
inline constexpr float kPadDeadZone = 0.33f;
// The right hand this close to the head (metres) turns Interact into the
// flashlight.
inline constexpr float kNearHead = 0.2f;

// The original's touchpad regions: where the pad is pressed, and which
// virtual button a click there is (down interact, up reload, centre radial).
// From: OpenVR PollInput, FUN_10007ad0.
struct TouchpadRegions {
    bool horizontal = false;   // |x| > |y|
    bool centre     = false;   // inside the dead zone
    bool interact = false, reload = false, radial = false;   // clicked regions
};
TouchpadRegions decode_touchpad(Vec2 pad, bool click) noexcept;

// Writes `c` into the player and UI action tiers (see gamepad.h), with
// snap-turn gating, grip toggles and the flashlight-near-head gesture.
void write_controls(const Controls& c, bool right_hand_near_head) noexcept;

// The controller layout in use: picks the button-prompt glyphs (the
// original's Vive wand set or its Oculus Touch set) and the controls help
// shown on the MotherVR options page.
enum class ControlScheme { Touchpad, Thumbstick, Index };
void set_control_scheme(ControlScheme scheme) noexcept;

// The controls help lines, "Action\tControl" (the original's text).
std::span<const char* const> control_help() noexcept;

} // namespace mothervr::input
