// OpenMotherVR-Provenance: clean
#include "core/hands/real_crouch.h"

#include "core/hands/tuning.h"
#include "game/constants.h"

#include <algorithm>

namespace omvr::core::hands {

namespace {

using util::seconds_between;
using util::TimePoint;

// A gap between two updates longer than this restarts the eye blend
// instead of easing across it (a menu, a loading screen).
constexpr Seconds kMaxBlendStep{0.1f};

} // namespace

void RealCrouch::reset() noexcept {
    wanted_.reset();
    crossing_ = false;
    check_.reset();
    resync_at_.reset();
}

void RealCrouch::press(bool crouch, TimePoint now, RealCrouchOutput& out) noexcept {
    out.press   = crouch ? CrouchPress::Crouch : CrouchPress::Stand;
    last_press_ = now;
    check_      = PendingCheck{crouch, now};
    // A crouch press is made once per crossing; a stand press is retried
    // (the game refuses to stand in a vent, 21 §11.5), but only once a press
    // of this session was followed, so a byte that does not follow the
    // game's crouch never makes the mod toggle it again and again.
    if (crouch || !followed_once_) crossing_ = false;
}

void RealCrouch::follow_press(bool game_crouched, TimePoint now, RealCrouchOutput& out) noexcept {
    if (!game_crouched) engaged_ = false;
    if (!check_) return;
    if (game_crouched == check_->crouch) {
        followed_once_ = true;
        if (check_->crouch) engaged_ = true;
        check_.reset();
    } else if (seconds_between(check_->at, now) >= kCrouchConfirm) {
        out.press_not_followed = true;
        check_.reset();
    }
}

void RealCrouch::ease_raise(bool raised, TimePoint now) noexcept {
    const float target = raised ? game::kCrouchCameraDropM : 0.0f;
    Seconds dt         = last_update_ ? seconds_between(*last_update_, now) : Seconds{0.0f};
    if (dt > kMaxBlendStep || dt < Seconds{0.0f}) dt = Seconds{0.0f};
    last_update_     = now;
    const float step = game::kCrouchCameraDropM * (dt / kCrouchEyeBlend);
    raise_m_ = raise_m_ < target ? std::min(target, raise_m_ + step) : std::max(target, raise_m_ - step);
}

RealCrouchOutput RealCrouch::update(const RealCrouchInput& in, TimePoint now) noexcept {
    RealCrouchOutput out;
    const float h = in.standing_eye_height_m;
    active_       = in.enabled && in.standing && h > 0.0f && in.head_height_m.has_value();
    if (!active_) {
        reset();
        engaged_ = engaged_ && in.game_crouched.value_or(false) && in.enabled;
        ease_raise(engaged_, now);
        out.eye_raise_m = raise_m_;
        return out;
    }
    out.crouch_below_m = kCrouchBelow * h;
    out.stand_above_m  = kStandAbove * h;
    const float head   = *in.head_height_m;

    // 21 §11.5: hysteresis between the two thresholds; the first reading
    // between them takes the game's own state.
    const bool was = wanted_.value_or(in.game_crouched.value_or(false));
    bool wanted    = was;
    if (head < out.crouch_below_m) wanted = true;
    if (head > out.stand_above_m) wanted = false;
    if (!wanted_ || wanted != *wanted_) crossing_ = true;
    wanted_ = wanted;

    if (in.game_crouched) follow_press(*in.game_crouched, now, out);

    if (!in.controllable || !in.game_crouched) {
        // 21 §12.2: no presses during an episode; on_hand_back re-syncs.
        crossing_ = false;
    } else if (resync_at_) {
        if (now >= *resync_at_) {
            resync_at_.reset();
            if (*in.game_crouched != wanted) press(wanted, now, out);
            crossing_ = false;
        }
    } else if (crossing_) {
        if (*in.game_crouched == wanted) {
            crossing_ = false;
        } else if (!last_press_ || seconds_between(*last_press_, now) >= kCrouchRetry) {
            press(wanted, now, out);
        }
    }

    ease_raise(engaged_ && in.game_crouched.value_or(false), now);
    out.eye_raise_m = raise_m_;
    return out;
}

void RealCrouch::on_hand_back(TimePoint now) noexcept {
    resync_at_ = util::after(now, kHandBackSettle);
    crossing_  = false;
}

} // namespace omvr::core::hands
