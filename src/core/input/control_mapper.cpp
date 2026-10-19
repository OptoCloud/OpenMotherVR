// VR controls into the gamepad action tiers.
//
// The tier offsets and the touchpad gestures are the original's (OpenVR
// PollInput, FUN_10007ad0; analysis dumps/06d_vr_input.md and 06b section 3);
// the thumbstick behaviour follows its Oculus Touch scheme (FUN_10004960).

#include "core/input/control_mapper.h"

#include "game/constants.h"
#include "core/input/controller_mapping.h"
#include "core/input/tuning.h"
#include "core/input/virtual_pad.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <string_view>

namespace omvr::core::input {

namespace {

// Map 0's analog action 2, `cam_vertical_rot`: the look stick's Y (04
// §5.8.5, 22 §2.1). A game fact, kept here beside the one row that writes it.
constexpr int kLookY = 2;

// What one frame of controls means for the game's actions, before it is
// written to the tiers (04 §5.8.5). Pure values.
struct GameActions {
    float move_x = 0, move_y = 0;
    float turn       = 0;
    float look_y     = 0;
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

GameActions game_actions(const Controls& c, bool near_head, bool snap_rotation, bool aim, bool tracker,
                         bool interacting) noexcept {
    const TouchpadRegions stick = decode_touchpad(c.turn, false);
    GameActions a;
    const Vec2 move = move_value(c);
    a.move_x        = move.x;
    a.move_y        = move.y;
    a.turn          = turn_value(c, stick, snap_rotation);
    // 04 §5.8.5 r10: a two-handed lever wants the look stick down, so during
    // an interaction the stick's Y is the game's look Y and cycles no weapon
    // (one owner per input). Outside interactions pitch comes from the head.
    a.look_y = interacting ? c.turn.y : 0.0f;
    // Sticks also select weapons up and down (04 §5.3).
    const bool weapon_stick = !c.touchpad_scheme && !stick.horizontal && !interacting;
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
        set_vr_digital(tier_, static_cast<std::size_t>(k), v);
        active_ |= v;
    }

    // Every analog value stays within the range the game's own pad
    // delivers, scaled or not: a runtime may report a stick slightly past 1
    // (spec 04 section 9). NaN fails both compares of clamp, so map it to 0
    // first.
    void analog(int j, float v) noexcept {
        v = std::isnan(v) ? 0.0f : std::clamp(v, -kAnalogLimit, kAnalogLimit);
        set_vr_analog(tier_, static_cast<std::size_t>(j), v);
        active_ |= std::fabs(v) > kActivityThreshold;
    }

    [[nodiscard]] bool active() const noexcept { return active_; }

private:
    ActionTier& tier_;
    bool active_ = false;
};

// 22 §8.4, §9: the holster actions held, the lean added to the stick's
// movement (the sum clamped as every analog value is, 04 §5.1) and the
// hold-breath input, merged with what the controls press.
void apply_gestures(GameActions& a, const GestureResult& g) noexcept {
    a.next_weapon |= g.next_weapon;
    a.prev_weapon |= g.prev_weapon;
    a.flashlight |= g.torch;
    a.tracker |= g.tracker;
    a.aim |= g.hold_breath;
    a.move_x = std::clamp(a.move_x + g.lean.x, -kAnalogLimit, kAnalogLimit);
    a.move_y = std::clamp(a.move_y + g.lean.y, -kAnalogLimit, kAnalogLimit);
}

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
    w.analog(kLookY, a.look_y);
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
    const bool enabled            = deps_.controls.dpad_emulation;
    const DpadEmulationResult emu = dpad_.update(in, enabled && deps_.stick.stick_free(), enabled,
                                                 enabled ? deps_.clock.now() : TimePoint{});
    Controls c                    = in;
    if (emu.stick_used) c.move = {};
    if (emu.modifier_held) c.sprint = false;
    c.dpad = in.dpad | emu.dpad; // 22 §7: a controller's own d-pad and the emulation both press
    deps_.pad.set_dpad(c.dpad);
    // Dev. Debug Options: each change of the press, so a tester's log shows
    // the d-pad reaching the game (11 §4.3).
    if (c.dpad != dpad_logged_) {
        dpad_logged_ = c.dpad;
        if (deps_.log.debug_enabled())
            diag::ports::logf(deps_.log, kLogDpad, c.dpad.up, c.dpad.down, c.dpad.left, c.dpad.right,
                              emu.dpad.any());
    }
    return c;
}

void ControlMapper::write(const Controls& in, bool near_head) noexcept {
    deps_.schemes.refresh(); // the d-pad glyphs follow Controls.DpadEmulation (22 §7)
    Controls c = apply_dpad(in);
    // 22 §8: the gestures see the controls as read; a grip a holster took
    // drives neither Aim nor the tracker, nor counts for ToggleGrips.
    // Run with every gesture off too, so each one's state resets (v1: no
    // effect).
    const GestureResult gestures = deps_.gestures.update(c, deps_.clock.now());
    if (gestures.grip_consumed[kLeftHand]) c.grip_l = false;
    if (gestures.grip_consumed[kRightHand]) c.grip_r = false;
    const bool toggle = deps_.input.toggle_grips;
    grips_.update(c.grip_r, c.grip_l, toggle);
    const bool aim      = toggle ? grips_.aim() : c.grip_r;
    const bool tracker  = toggle ? grips_.tracker() : c.grip_l && !c.radial;
    GameActions actions = game_actions(c, near_head, deps_.input.snap_rotation, aim, tracker,
                                       deps_.interactions.in_interaction());
    apply_gestures(actions, gestures);

    auto& tiers = deps_.pad.tiers();
    TierWriter player{tiers[game::kPlayerMap]};
    TierWriter menu{tiers[game::kMenuMap]};
    write_player_map(player, c, actions);
    write_menu_map(menu, c, actions);

    if (player.active() || menu.active() || c.dpad.any()) deps_.pad.note_vr_activity();
}

void ControlMapper::set_scheme(ControlScheme scheme) noexcept { deps_.schemes.set(scheme); }

void ControlMapper::identify_controller(std::size_t hand, ControllerSource source,
                                        std::string_view id) noexcept {
    deps_.schemes.identify(hand, source, id);
}

} // namespace omvr::core::input
