// VR controls into the gamepad action tiers.
//
// The tier offsets and the touchpad gestures are the original's (OpenVR
// PollInput, FUN_10007ad0; analysis dumps/06d_vr_input.md and 06b section 3);
// the thumbstick behaviour follows its Oculus Touch scheme (FUN_10004960).

#include "vr_controls.h"

#include "config/config.h"
#include "gamepad.h"

#include <cmath>

namespace mothervr::input {

namespace {

constexpr float kTurnScale   = 0.85f;  // touchpad turn
constexpr float kMoveScale   = 1.1f;   // touchpad move
constexpr float kActive      = 0.85f;  // analog magnitude that counts as activity

// Grip toggles flip on the rising edge. Original: DAT_1006af41 (right),
// DAT_1006af40 (left).
bool g_prev_grip_r = false, g_prev_grip_l = false;
bool g_aim_toggled = false, g_tracker_toggled = false;

} // namespace

TouchpadRegions decode_touchpad(Vec2 pad, bool click) noexcept {
    TouchpadRegions r;
    r.horizontal = std::fabs(pad.x) > std::fabs(pad.y);
    r.centre     = std::fabs(pad.x) < kPadDeadZone && std::fabs(pad.y) < kPadDeadZone;
    const bool vertical_edge = click && !r.horizontal && !r.centre;
    r.interact = vertical_edge && pad.y < 0.0f;
    r.reload   = vertical_edge && pad.y > 0.0f;
    r.radial   = click && r.centre;
    return r;
}

void write_controls(const Controls& c, bool near_head) noexcept {
    const auto& cfg = config::current().input;

    const float tx = c.turn.x, ty = c.turn.y;
    const auto pad = decode_touchpad(c.turn, false);
    const bool horiz = pad.horizontal, centre = pad.centre;
    // With snap rotation a touchpad turn needs a click; sticks just push.
    const float turn = (cfg.snap_rotation && !c.turn_engaged) ? 0.0f
                     : (horiz && !centre) ? tx * (c.touchpad_scheme ? kTurnScale : 1.0f) : 0.0f;
    // Sticks also select weapons up and down.
    const bool next_weapon = !c.touchpad_scheme && !horiz && ty > 0.5f;
    const bool prev_weapon = !c.touchpad_scheme && !horiz && ty < -0.5f;
    const float move_scale = c.touchpad_scheme ? kMoveScale : 1.0f;
    const bool flashlight = c.flashlight || (c.interact && near_head);
    const bool confirm = c.interact || (c.touchpad_scheme && c.sprint);

    if (cfg.toggle_grips) {
        if (c.grip_r && !g_prev_grip_r) g_aim_toggled = !g_aim_toggled;
        if (c.grip_l && !g_prev_grip_l) g_tracker_toggled = !g_tracker_toggled;
    }
    g_prev_grip_r = c.grip_r;
    g_prev_grip_l = c.grip_l;
    const bool aim     = cfg.toggle_grips ? g_aim_toggled : c.grip_r;
    const bool tracker = cfg.toggle_grips ? g_tracker_toggled : c.grip_l && !c.radial;

    auto& t = tiers();
    bool active = false;
    auto digital = [&active](ActionTier& tier, int k, bool v) {
        tier.digital[k] = v;
        active |= v;
    };
    auto analog = [&active](ActionTier& tier, int j, float v) {
        tier.analog[j] = v;
        active |= std::fabs(v) > kActive;
    };

    // Player map (names from the controls help).
    auto& a = t[0];
    digital(a, 0x08, c.sprint);                // Sprint
    digital(a, 0x09, c.trigger_l > 0.5f);      // Crouch
    digital(a, 0x0A, aim);                     // Aim
    digital(a, 0x0B, c.trigger_r > 0.5f);      // Fire / Throw / Melee
    digital(a, 0x0E, c.trigger_r > 0.5f);
    digital(a, 0x0C, c.interact);              // Interact
    digital(a, 0x0F, c.interact);
    digital(a, 0x0D, c.radial);                // Radial menu
    digital(a, 0x10, c.reload);                // Reload / Activate
    digital(a, 0x11, c.reload);
    digital(a, 0x1A, prev_weapon);             // Previous weapon
    digital(a, 0x1B, next_weapon);             // Next weapon
    digital(a, 0x1C, tracker);                 // Motion tracker
    digital(a, 0x1D, flashlight);              // Flashlight
    digital(a, 0x26, c.map);                   // Map
    digital(a, 0x27, c.grip_l);
    digital(a, 0x28, c.grip_r);
    analog(a, 0, c.move.x * move_scale);       // Move
    analog(a, 1, c.move.y * move_scale);
    analog(a, 3, turn);                        // Turn

    // UI map.
    auto& b = t[1];
    digital(b, 0x00, confirm);                 // confirm
    digital(b, 0x10, confirm);
    digital(b, 0x01, c.radial);
    digital(b, 0x08, c.pause);                 // Pause
    digital(b, 0x09, c.grip_l);
    digital(b, 0x0A, c.reload);                // back
    digital(b, 0x0F, c.reload);
    digital(b, 0x0B, flashlight);
    digital(b, 0x0E, flashlight);
    analog(b, 0, c.trigger_l);
    analog(b, 1, c.trigger_r);
    analog(b, 2, c.move.x);
    analog(b, 3, c.move.y);
    analog(b, 4, tx);
    analog(b, 5, ty);

    if (active) note_vr_activity();
}

namespace {

// The original's help text for its two schemes (OpenVR and Oculus backends).
constexpr const char* kTouchpadHelp[] = {
    "Move\tL Touchpad",
    "Sprint\tL Touchpad Click",
    "Crouch\tL Trigger",
    "Turn\tR Touchpad L/R",
    "Aim\tR Grip",
    "Fire / Throw / Melee\tR Trigger",
    "Motion Tracker\tL Grip",
    "Interact\tR Touchpad Down",
    "Radial Menu\tR Touchpad Center",
    "Reload / Activate\tR Touchpad Up",
    "Flashlight [Press Interact near Head]\tR Touchpad Down",
    "Map Menu\tL Menu",
    "Pause Menu\tR Menu",
};
constexpr const char* kThumbstickHelp[] = {
    "Move\tLeft Thumbstick",
    "Sprint\tLeft Thumbstick Click",
    "Crouch\tLeft Trigger",
    "Turn\tRight Thumbstick L/R",
    "Aim\tRight Grip",
    "Fire / Throw / Melee\tRight Trigger",
    "Motion Tracker\tLeft Grip",
    "Interact\tA",
    "Radial Menu\tRight Thumbstick Click",
    "Reload / Activate\tB",
    "Flashlight\tY",
    "Flashlight [Press Interact near Head]\tA",
    "Next Weapon\tRight Thumbstick Up",
    "Previous Weapon\tRight Thumbstick Down",
    "Map Menu\tX",
    "Pause Menu\tMenu Button",
};
// Index has no menu button: the left trackpad press pauses.
constexpr const char* kIndexHelp[] = {
    "Move\tLeft Thumbstick",
    "Sprint\tLeft Thumbstick Click",
    "Crouch\tLeft Trigger",
    "Turn\tRight Thumbstick L/R",
    "Aim\tRight Grip",
    "Fire / Throw / Melee\tRight Trigger",
    "Motion Tracker\tLeft Grip",
    "Interact\tA",
    "Radial Menu\tRight Thumbstick Click",
    "Reload / Activate\tB",
    "Flashlight\tY",
    "Flashlight [Press Interact near Head]\tA",
    "Next Weapon\tRight Thumbstick Up",
    "Previous Weapon\tRight Thumbstick Down",
    "Map Menu\tX",
    "Pause Menu\tLeft Trackpad Press",
};

std::span<const char* const> g_help = kTouchpadHelp;

void set_glyph_scheme(bool touchpad) noexcept {
    if (touchpad) {
        set_glyph(0, "VR_TP_D");   // A: interact
        set_glyph(1, "VR_TP_C");   // B: radial
        set_glyph(2, "VR_TP_U");   // X: reload
        set_glyph(3, "Y");
        set_glyph(8, "VR_MENU");   // BACK: map
    } else {
        set_glyph(0, "VR_A");
        set_glyph(1, "VR_RSC");
        set_glyph(2, "VR_B");
        set_glyph(3, "VR_Y");      // Y: flashlight
        set_glyph(8, "VR_X");
    }
    set_glyph(9, "VR_MENU");       // START: pause
    set_glyph(10, "VR_LG");        // LB: motion tracker
    set_glyph(11, "VR_RG");        // RB: aim
}

} // namespace

void set_control_scheme(ControlScheme scheme) noexcept {
    set_glyph_scheme(scheme == ControlScheme::Touchpad);
    g_help = scheme == ControlScheme::Touchpad ? std::span<const char* const>(kTouchpadHelp)
           : scheme == ControlScheme::Index    ? std::span<const char* const>(kIndexHelp)
                                               : std::span<const char* const>(kThumbstickHelp);
}

std::span<const char* const> control_help() noexcept { return g_help; }

} // namespace mothervr::input
