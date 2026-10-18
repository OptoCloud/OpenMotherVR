// OpenMotherVR-Provenance: clean
#include "core/input/gestures.h"

#include "core/input/tuning.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <string_view>
#include <utility>

namespace omvr::core::input {

namespace {

constexpr std::string_view kLogRecenter = "[input] gesture: recenter (both hands at the head, grips held)";
constexpr std::string_view kLogRecenterSuppressed = "[input] gesture: recenter suppressed: {}";
constexpr std::string_view kLogHolster            = "[input] gesture: holster {} ({} hand): {}";
constexpr std::string_view kLogHolsterSuppressed  = "[input] gesture: holster {} ({} hand) suppressed: {}";
constexpr std::string_view kLogLeanReference      = "[input] gesture: hiding lean reference taken";
constexpr std::string_view kLogLeanOn    = "[input] gesture: hiding lean (side {:.2f}, forward {:.2f})";
constexpr std::string_view kLogLeanOff   = "[input] gesture: hiding lean released";
constexpr std::string_view kLogBreathOn  = "[input] gesture: hold breath (hand over the mouth)";
constexpr std::string_view kLogBreathOff = "[input] gesture: hold breath released";

constexpr std::string_view kReasonFire        = "Fire pressed";
constexpr std::string_view kReasonFocus       = "no input focus, or focus returned under 0.5 s ago";
constexpr std::string_view kReasonMenu        = "menu";
constexpr std::string_view kReasonInteraction = "interaction";

std::string_view zone_name(Holster z) noexcept {
    switch (z) {
        case Holster::RightShoulder: return "right shoulder";
        case Holster::LeftShoulder: return "left shoulder";
        case Holster::LeftHip: return "left hip";
        case Holster::Chest: return "chest";
        case Holster::None: break;
    }
    return "none";
}

// The map 0 action each zone drives (22 §8.4, names of 22 §2.1).
std::string_view action_name(Holster z) noexcept {
    switch (z) {
        case Holster::RightShoulder: return "next_main_weapon";
        case Holster::LeftShoulder: return "prev_main_weapon";
        case Holster::LeftHip: return "select_torch";
        case Holster::Chest: return "select_motion_tracker";
        case Holster::None: break;
    }
    return "none";
}

bool lean_active(const Vec2& v) noexcept { return v.x != 0.0f || v.y != 0.0f; }

} // namespace

// ---- recenter (22 §8.2) --------------------------------------------------------

RecenterGesture::Result RecenterGesture::update(bool enabled, const Input& in, TimePoint now) noexcept {
    if (!enabled || !(in.grips && in.near)) {
        since_.reset();
        fired_      = false;
        suppressed_ = false;
        return Result::None;
    }
    if (fired_) return Result::None;
    if (in.fire || !in.open) {
        since_.reset();
        if (std::exchange(suppressed_, true)) return Result::None;
        return in.fire ? Result::SuppressedFire : Result::SuppressedFocus;
    }
    if (!since_) since_ = now;
    if (now - *since_ < kRecenterHold) return Result::None;
    since_.reset();
    fired_ = true;
    return Result::Fired;
}

// ---- holsters (22 §8.4) --------------------------------------------------------

Holster holster_zone(std::size_t hand, const Vec3& p) noexcept {
    const bool right = hand == kRightHand;
    if (p.y > -kShoulderBelowHead && p.z > kShoulderBehind)
        return right ? Holster::RightShoulder : Holster::LeftShoulder;
    if (!right && p.y < -kHipBelowHead && p.x < -kHipLeftOf && p.z > -kHipFrontMost) return Holster::LeftHip;
    // Close to the chest only: the hold in front of the body is not in it.
    if (p.y <= -kChestBelowTop && p.y >= -kChestBelowBottom && std::fabs(p.x) <= kChestHalfWidth &&
        p.z >= -kChestFrontMost && p.z <= kShoulderBehind)
        return Holster::Chest;
    return Holster::None;
}

Holsters::Result Holsters::update(bool enabled, const Input& in) noexcept {
    Result r;
    for (std::size_t h = 0; h < in.hands.size(); ++h) {
        const HandInput& hand = in.hands[h];
        const bool press      = hand.grip && !prev_grip_[h];
        prev_grip_[h]         = hand.grip;
        if (!hand.grip || !enabled) {
            // Released, or the gesture turned off: the grip is the player's again.
            held_[h]     = Holster::None;
            consumed_[h] = false;
            continue;
        }
        if (press) {
            const Holster zone = hand.position ? holster_zone(h, *hand.position) : Holster::None;
            if (zone != Holster::None) {
                const bool fire = !in.blocked && in.open;
                r.event         = Event{.hand = h, .zone = zone, .fired = fire};
                if (fire) {
                    held_[h]     = zone;
                    consumed_[h] = true;
                }
            }
        }
        // Off during interactions and menus: the action lets go, the grip
        // stays the holster's until released.
        if (in.blocked || !in.open) held_[h] = Holster::None;
    }
    r.held     = held_;
    r.consumed = consumed_;
    return r;
}

// ---- lean (22 §9.2) ------------------------------------------------------------

float LeanAxis::update(float v) noexcept {
    const float a = std::fabs(v);
    if (active_) {
        if (a < kLeanStop) active_ = false;
    } else if (a > kLeanStart) {
        active_ = true;
    }
    if (!active_) return 0.0f;
    return std::copysign(std::min(a / kLeanFull, 1.0f), v);
}

void HidingLean::reset() noexcept {
    reference_.reset();
    side_.reset();
    forward_.reset();
    up_.reset();
}

HidingLean::Result HidingLean::update(bool enabled, const Input& in) noexcept {
    Result r;
    if (!enabled || !in.hiding || in.poses == nullptr) {
        reset();
        return r;
    }
    const BodyPoses& p = *in.poses;
    if (!reference_ && p.head_valid) {
        reference_        = HeadFrame{p.head, p.head_yaw};
        r.reference_taken = true;
    }
    if (!reference_ || in.menu || !in.open || !p.head_valid) {
        side_.reset();
        forward_.reset();
        up_.reset();
        return r;
    }
    const Vec3 o        = reference_->local(p.head);
    const float side    = side_.update(o.x);
    const float forward = forward_.update(-o.z);
    // A rise peeks out as a forward lean does; a duck is no lean.
    const float up = std::max(up_.update(o.y), 0.0f);
    r.move         = {side, std::fabs(forward) >= up ? forward : up};
    return r;
}

// ---- hold breath (22 §9.3) -----------------------------------------------------

bool HoldBreath::update(bool enabled, const Input& in) noexcept {
    if (!enabled || !in.breathing_game || in.menu || !in.open || in.poses == nullptr) {
        held_ = false;
        return false;
    }
    constexpr Vec3 kMouth{0.0f, -kMouthBelowHead, -kMouthInFront};
    float nearest = std::numeric_limits<float>::infinity();
    for (std::size_t h = 0; h < in.poses->hands.size(); ++h) {
        if (const auto p = hand_in_head_frame(*in.poses, h))
            nearest = std::min(nearest, util::length(util::sub(*p, kMouth)));
    }
    held_ = held_ ? nearest <= kMouthRelease : nearest < kMouthPress;
    return held_;
}

// ---- the set -------------------------------------------------------------------

GestureResult Gestures::update(const Controls& c, TimePoint now) noexcept {
    GestureResult out;
    const bool open               = gate_.open(c.poses.focused, now);
    const ports::GameContext game = deps_.context.game_context();
    apply_holsters(c, game, open, out);
    Controls held = c; // the grips a holster took are not the recenter's
    if (out.grip_consumed[kLeftHand]) held.grip_l = false;
    if (out.grip_consumed[kRightHand]) held.grip_r = false;
    run_recenter(held, open, now);
    apply_lean(c, game, open, out);
    apply_breath(c, game, open, out);
    return out;
}

void Gestures::run_recenter(const Controls& c, bool open, TimePoint now) noexcept {
    const RecenterGesture::Input in{
        .grips = c.grip_l && c.grip_r,
        .near  = hand_near_head(c.poses, kLeftHand, kRecenterHandDistance) &&
                 hand_near_head(c.poses, kRightHand, kRecenterHandDistance),
        .fire  = c.trigger_r > kTriggerPressed,
        .open  = open,
    };
    switch (recenter_.update(deps_.controls.recenter_gesture, in, now)) {
        case RecenterGesture::Result::Fired:
            recenter_request_.store(true, std::memory_order_release);
            deps_.log.line(kLogRecenter);
            break;
        case RecenterGesture::Result::SuppressedFire:
            if (deps_.log.debug_enabled()) diag::ports::logf(deps_.log, kLogRecenterSuppressed, kReasonFire);
            break;
        case RecenterGesture::Result::SuppressedFocus:
            if (deps_.log.debug_enabled()) diag::ports::logf(deps_.log, kLogRecenterSuppressed, kReasonFocus);
            break;
        case RecenterGesture::Result::None: break;
    }
}

void Gestures::apply_holsters(const Controls& c, const ports::GameContext& game, bool open,
                              GestureResult& out) noexcept {
    const Holsters::Input in{
        .hands = {Holsters::HandInput{.grip = c.grip_l, .position = hand_in_head_frame(c.poses, kLeftHand)},
                  Holsters::HandInput{.grip = c.grip_r, .position = hand_in_head_frame(c.poses, kRightHand)}},
        .blocked = game.menu || game.interaction,
        .open    = open,
    };
    const Holsters::Result r = holsters_.update(deps_.controls.holster_gestures, in);
    out.grip_consumed        = r.consumed;
    for (const Holster z : r.held) {
        out.next_weapon |= z == Holster::RightShoulder;
        out.prev_weapon |= z == Holster::LeftShoulder;
        out.torch |= z == Holster::LeftHip;
        out.tracker |= z == Holster::Chest;
    }
    if (!r.event) return;
    const auto& e = *r.event;
    if (e.fired) {
        diag::ports::logf(deps_.log, kLogHolster, zone_name(e.zone), hand_name(e.hand), action_name(e.zone));
    } else if (deps_.log.debug_enabled()) {
        const std::string_view why = !open ? kReasonFocus : game.menu ? kReasonMenu : kReasonInteraction;
        diag::ports::logf(deps_.log, kLogHolsterSuppressed, zone_name(e.zone), hand_name(e.hand), why);
    }
}

void Gestures::apply_lean(const Controls& c, const ports::GameContext& game, bool open,
                          GestureResult& out) noexcept {
    const HidingLean::Result r =
        lean_.update(deps_.controls.hiding_lean,
                     {.hiding = game.hiding, .menu = game.menu, .open = open, .poses = &c.poses});
    out.lean = r.move;
    if (r.reference_taken && deps_.log.debug_enabled()) deps_.log.line(kLogLeanReference);
    // Once per change (22 §16), never per frame.
    const bool active = lean_active(r.move);
    if (active == lean_logged_) return;
    lean_logged_ = active;
    if (active)
        diag::ports::logf(deps_.log, kLogLeanOn, r.move.x, r.move.y);
    else
        deps_.log.line(kLogLeanOff);
}

void Gestures::apply_breath(const Controls& c, const ports::GameContext& game, bool open,
                            GestureResult& out) noexcept {
    out.hold_breath = breath_.update(
        deps_.controls.hold_breath_gesture,
        {.breathing_game = game.breathing_game, .menu = game.menu, .open = open, .poses = &c.poses});
    if (out.hold_breath == breath_logged_) return;
    breath_logged_ = out.hold_breath;
    deps_.log.line(out.hold_breath ? kLogBreathOn : kLogBreathOff);
}

} // namespace omvr::core::input
