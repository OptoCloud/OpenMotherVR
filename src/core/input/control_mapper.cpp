// VR controls into the gamepad action tiers.
//
// The tier offsets and the touchpad gestures are the original's (OpenVR
// PollInput, FUN_10007ad0; analysis dumps/06d_vr_input.md and 06b section 3);
// the thumbstick behaviour follows its Oculus Touch scheme (FUN_10004960).

#include "core/input/control_mapper.h"

#include "core/config/settings_store.h"
#include "core/input/touchpad.h"
#include "core/input/tuning.h"
#include "core/input/virtual_pad.h"

#include <algorithm>
#include <cmath>

namespace omvr::core::input {

void ControlMapper::write(const Controls& c, bool near_head) noexcept {
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

    auto& t = pad_.tiers();
    bool active = false;
    auto digital = [&active](ActionTier& tier, int k, bool v) {
        tier.digital[k] = v;
        active |= v;
    };
    // Every analog value stays within the range the game's own pad delivers,
    // scaled or not: a runtime may report a stick slightly past 1 (spec 04
    // section 9). NaN fails both compares of clamp, so map it to 0 first.
    auto analog = [&active](ActionTier& tier, int j, float v) {
        v = std::isnan(v) ? 0.0f : std::clamp(v, -1.0f, 1.0f);
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
    // The touchpad's scale reaches full speed short of the rim; keep the
    // result on the unit disc the game expects of a stick.
    float move_x = c.move.x * move_scale, move_y = c.move.y * move_scale;
    if (const float len = std::hypot(move_x, move_y); len > 1.0f) {
        move_x /= len;
        move_y /= len;
    }
    analog(a, 0, move_x);                      // Move
    analog(a, 1, move_y);
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

    if (active) pad_.note_vr_activity();
}

void ControlMapper::set_scheme(ControlScheme scheme) noexcept { schemes_.set(scheme); }

} // namespace omvr::core::input
