// VR controls into the gamepad action tiers.
//
// The tier offsets and the touchpad gestures are the original's (OpenVR
// PollInput, FUN_10007ad0; analysis dumps/06d_vr_input.md and 06b section 3);
// the thumbstick behaviour follows its Oculus Touch scheme (FUN_10004960).

#include "core/input/control_mapper.h"

#include "game/constants.h"
#include "core/input/touchpad.h"
#include "core/input/tuning.h"
#include "core/input/virtual_pad.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <string_view>

namespace omvr::core::input {

namespace {

// What one frame of controls means for the game's actions, before it is
// written to the tiers (04 §5.8.5). Pure values.
struct GameActions {
    float move_x = 0, move_y = 0;
    float turn       = 0;
    bool next_weapon = false, prev_weapon = false;
    bool flashlight = false;
    bool confirm    = false;
    bool aim = false, tracker = false;
};

// 04 §5.8.5 Turn: with snap rotation a touchpad turn needs a click; sticks
// just push. Only a horizontal push outside the centre turns.
float turn_value(const Controls& c, const TouchpadRegions& stick, bool snap_rotation) noexcept {
    if (snap_rotation && !c.turn_engaged) return 0.0f;
    if (!stick.horizontal || stick.centre) return 0.0f;
    return c.turn.x * (c.touchpad_scheme ? kTouchpadTurnScale : 1.0f);
}

// 04 §5.2: the touchpad's scale reaches full speed short of the rim; the
// result stays on the unit disc the game expects of a stick, so a diagonal
// is never faster than straight ahead.
Vec2 move_value(const Controls& c) noexcept {
    const float scale = c.touchpad_scheme ? kTouchpadMoveScale : 1.0f;
    Vec2 move{c.move.x * scale, c.move.y * scale};
    if (const float len = std::hypot(move.x, move.y); len > 1.0f) {
        move.x /= len;
        move.y /= len;
    }
    return move;
}

GameActions game_actions(const Controls& c, bool near_head, bool snap_rotation, bool aim,
                         bool tracker) noexcept {
    const TouchpadRegions stick = decode_touchpad(c.turn, false);
    GameActions a;
    const Vec2 move = move_value(c);
    a.move_x        = move.x;
    a.move_y        = move.y;
    a.turn          = turn_value(c, stick, snap_rotation);
    // Sticks also select weapons up and down (04 §5.3).
    const bool weapon_stick = !c.touchpad_scheme && !stick.horizontal;
    a.next_weapon           = weapon_stick && c.turn.y > kWeaponStickDeflection;
    a.prev_weapon           = weapon_stick && c.turn.y < -kWeaponStickDeflection;
    a.flashlight            = c.flashlight || (c.interact && near_head);
    a.confirm               = c.interact || (c.touchpad_scheme && c.sprint);
    a.aim                   = aim;
    a.tracker               = tracker;
    return a;
}

// Writes action values into one tier and notes whether any of them counts
// as VR activity (04 §5.1).
class TierWriter {
public:
    explicit TierWriter(ActionTier& tier) noexcept : tier_(tier) {}

    void digital(int k, bool v) noexcept {
        tier_.digital[static_cast<std::size_t>(k)] = v;
        active_ |= v;
    }

    // Every analog value stays within the range the game's own pad
    // delivers, scaled or not: a runtime may report a stick slightly past 1
    // (spec 04 section 9). NaN fails both compares of clamp, so map it to 0
    // first.
    void analog(int j, float v) noexcept {
        v = std::isnan(v) ? 0.0f : std::clamp(v, -kAnalogLimit, kAnalogLimit);
        tier_.analog[static_cast<std::size_t>(j)] = v;
        active_ |= std::fabs(v) > kActivityThreshold;
    }

    [[nodiscard]] bool active() const noexcept { return active_; }

private:
    ActionTier& tier_;
    bool active_ = false;
};

// Player map (names from the controls help, 04 §2.4).
void write_player_map(TierWriter& w, const Controls& c, const GameActions& a) noexcept {
    using namespace game::player_action;
    const bool fire = c.trigger_r > kTriggerPressed;
    w.digital(kSprint, c.sprint);
    w.digital(kCrouch, c.trigger_l > kTriggerPressed);
    w.digital(kAim, a.aim);
    w.digital(kFire, fire);
    w.digital(kFire2, fire);
    w.digital(kInteract, c.interact);
    w.digital(kInteract2, c.interact);
    w.digital(kRadial, c.radial);
    w.digital(kReload, c.reload);
    w.digital(kReload2, c.reload);
    w.digital(kPrevWeapon, a.prev_weapon);
    w.digital(kNextWeapon, a.next_weapon);
    w.digital(kTracker, a.tracker);
    w.digital(kFlashlight, a.flashlight);
    w.digital(kMap, c.map);
    w.digital(kLeftGripHeld, c.grip_l);
    w.digital(kRightGripHeld, c.grip_r);
    w.analog(kMoveX, a.move_x);
    w.analog(kMoveY, a.move_y);
    w.analog(kTurn, a.turn);
}

// Menu map (04 §2.4).
void write_menu_map(TierWriter& w, const Controls& c, const GameActions& a) noexcept {
    using namespace game::menu_action;
    w.digital(kConfirm, a.confirm);
    w.digital(kConfirm2, a.confirm);
    w.digital(kRadial, c.radial);
    w.digital(kPause, c.pause);
    w.digital(kLeftGripHeld, c.grip_l);
    w.digital(kBack, c.reload);
    w.digital(kBack2, c.reload);
    w.digital(kFlashlight, a.flashlight);
    w.digital(kFlashlight2, a.flashlight);
    w.analog(kTriggerL, c.trigger_l);
    w.analog(kTriggerR, c.trigger_r);
    w.analog(kMoveX, c.move.x);
    w.analog(kMoveY, c.move.y);
    w.analog(kTurnX, c.turn.x);
    w.analog(kTurnY, c.turn.y);
}

constexpr std::string_view kLogDpad = "[input] d-pad up {} down {} left {} right {} (emulated {})";

// 22 §7: a controller's own d-pad and the emulation both press.
Dpad either(Dpad a, Dpad b) noexcept {
    return {
        .up = a.up || b.up, .down = a.down || b.down, .left = a.left || b.left, .right = a.right || b.right};
}

} // namespace

void ControlMapper::GripToggles::update(bool grip_r, bool grip_l, bool toggle) noexcept {
    if (toggle) {
        if (grip_r && !prev_grip_r_) aim_ = !aim_;
        if (grip_l && !prev_grip_l_) tracker_ = !tracker_;
    }
    prev_grip_r_ = grip_r;
    prev_grip_l_ = grip_l;
}

// 22 §7: the emulated d-pad takes the left stick from walking while it
// presses, and its modifier from sprint while held. The press itself goes to
// the pad, which drives the actions on the d-pad by their stock control.
Controls ControlMapper::apply_dpad(const Controls& in) noexcept {
    const bool enabled = controls_.dpad_emulation;
    const DpadEmulationResult emu =
        dpad_.update(in, enabled && stick_.stick_free(), enabled,
                     enabled ? clock_.now() : std::chrono::steady_clock::time_point{});
    Controls c = in;
    if (emu.stick_used) c.move = {};
    if (emu.modifier_held) c.sprint = false;
    c.dpad = either(in.dpad, emu.dpad);
    pad_.set_dpad(c.dpad);
    // Dev. Debug Options: each change of the press, so a tester's log shows
    // the d-pad reaching the game (11 §4.3).
    if (c.dpad != dpad_logged_) {
        dpad_logged_ = c.dpad;
        if (log_.debug_enabled())
            diag::ports::logf(log_, kLogDpad, c.dpad.up, c.dpad.down, c.dpad.left, c.dpad.right,
                              emu.dpad.any());
    }
    return c;
}

void ControlMapper::write(const Controls& in, bool near_head) noexcept {
    const Controls c  = apply_dpad(in);
    const bool toggle = settings_.toggle_grips;
    grips_.update(c.grip_r, c.grip_l, toggle);
    const bool aim            = toggle ? grips_.aim() : c.grip_r;
    const bool tracker        = toggle ? grips_.tracker() : c.grip_l && !c.radial;
    const GameActions actions = game_actions(c, near_head, settings_.snap_rotation, aim, tracker);

    auto& tiers = pad_.tiers();
    TierWriter player{tiers[game::kPlayerMap]};
    TierWriter menu{tiers[game::kMenuMap]};
    write_player_map(player, c, actions);
    write_menu_map(menu, c, actions);

    if (player.active() || menu.active() || c.dpad.any()) pad_.note_vr_activity();
}

void ControlMapper::set_scheme(ControlScheme scheme) noexcept { schemes_.set(scheme); }

} // namespace omvr::core::input
