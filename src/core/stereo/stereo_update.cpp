// Stereo::Update replaced (05 §3.2). Analysis:
// dumps/06c_detours_and_patches.md (sections 3 and 4).

#include "core/stereo/stereo_update.h"

#include "core/stereo/snap_turn.h"
#include "core/stereo/tuning.h"

#include <algorithm>
#include <cmath>

namespace omvr::core::stereo {

UpdateOutput StereoUpdate::run(const UpdateInput& in) noexcept {
    UpdateOutput out;
    const config::ComfortSettings comfort = settings_.comfort;
    const Turning turn                    = turning(comfort);
    const TurnBlock blocked               = block(in, out);
    report_turning(turn, comfort, out);
    bool smoothed = false;
    if (in.pad) {
        // 05 §3.2b: the pad in use switches game look off again.
        if (in.pad_activity) keyboard_mouse_ = false;
        if (turn == Turning::Snap && blocked != TurnBlock::None) hold_snap(in);
        if (turn == Turning::Snap && blocked == TurnBlock::None) snap_turn(in, out, comfort);
        if (turn == Turning::Smooth && blocked == TurnBlock::None)
            smoothed = smooth_turn(in, out, comfort.smooth_turn_speed);
    }
    // Spec 22 §4.2, §4.4: smooth turn resumes with no step after anything
    // that stopped it, a block included.
    if (!smoothed) smooth_time_.reset();
    if (in.keyboard_and_mouse && in.key_or_mouse_activity) keyboard_mouse_ = true;
    // Spec 22 §4.1: the game's own stick look only when it is the turning,
    // so the stick never turns twice, and not while turning is blocked
    // (§4.4; mouse look stays).
    if (in.follow_active)
        out.look_enabled = (turn == Turning::GameLook && blocked == TurnBlock::None) || keyboard_mouse_;
    // Spec 22 §4.3: the fade follows the step the yaw write makes.
    out.snap_fade       = turn == Turning::Snap && out.yaw.has_value() && comfort.snap_turn_fade;
    out.vignette_target = vignette_motion(in, turn, blocked, out) ? comfort.vignette : 0.0f;
    out.last_snap       = last_snap_;
    out.keyboard_mouse  = keyboard_mouse_;

    recenter(in, out);

    // 05 §3.2g: never black during cutscenes and fades, black while a
    // loading screen is up.
    out.scene_visibility = scene_.update(in.follow_active, in.mode);
    if (loading_blackout_) out.scene_visibility = kSceneBlack;
    return out;
}

Turning StereoUpdate::turning(const config::ComfortSettings& comfort) const noexcept {
    if (settings_.input.snap_rotation) return Turning::Snap;
    return comfort.smooth_turn_speed > 0.0f ? Turning::Smooth : Turning::GameLook;
}

TurnBlock StereoUpdate::block(const UpdateInput& in, UpdateOutput& out) noexcept {
    TurnBlockChange now{};
    if (settings_.controls.interaction_aware_turning) {
        now.block = turn_block({.in_interaction   = in.in_interaction,
                                .interaction_kind = in.interaction_kind,
                                .breathing_game   = in.breathing_game,
                                .cutscene_or_fade = scene_.cutscene_or_fade()});
        if (now.block == TurnBlock::Interaction) now.kind = in.interaction_kind;
    }
    if (now != reported_block_) {
        reported_block_   = now;
        out.block_changed = now;
    }
    return now.block;
}

// Spec 22 §4.4: as when the camera becomes inactive (04 §6), a stick held
// through the block must come back inside the threshold before it steps.
void StereoUpdate::hold_snap(const UpdateInput& in) noexcept {
    snap_latch_ = turn_out(in.turn);
    snap_hold_  = snap_latch_;
    snap_time_  = {};
}

void StereoUpdate::report_turning(Turning turn, const config::ComfortSettings& comfort,
                                  UpdateOutput& out) noexcept {
    const TurningInUse now{.turning     = turn,
                           .repeat_ms   = turn == Turning::Snap ? comfort.snap_turn_repeat_ms : 0,
                           .speed_deg_s = turn == Turning::Smooth ? comfort.smooth_turn_speed : 0.0f};
    if (reported_turning_.changed(now)) out.turning_changed = now;
}

// Spec 22 §5: the merged stick movement past its threshold, smooth turn
// changing the yaw, or the game's own look turning with the stick. Not
// snap steps (they have the fade), not the keyboard, and never without a
// first-person camera or in the menu screen's mode.
bool StereoUpdate::vignette_motion(const UpdateInput& in, Turning turn, TurnBlock blocked,
                                   const UpdateOutput& out) noexcept {
    if (!in.pad || !in.follow_active || in.mode == StereoMode::Screen) return false;
    if (std::hypot(in.move_x, in.move_y) > kVignetteMoveThreshold) return true;
    if (turn == Turning::Smooth) return out.yaw.has_value();
    return turn == Turning::GameLook && blocked == TurnBlock::None && std::abs(in.turn) > kSmoothTurnDeadZone;
}

// 05 §3.2c: one step per push of the stick; a held stick repeats after the
// comfort repeat interval (spec 22 §4.3; 0 never repeats). Not in the menu
// screen's mode, where the game has no camera to turn.
void StereoUpdate::snap_turn(const UpdateInput& in, UpdateOutput& out,
                             const config::ComfortSettings& comfort) noexcept {
    const auto& input           = settings_.input;
    const bool out_of_dead_zone = turn_out(in.turn);
    const auto repeat           = std::chrono::milliseconds(comfort.snap_turn_repeat_ms);
    const bool repeats          = comfort.snap_turn_repeat_ms > 0;
    if (!out_of_dead_zone) snap_hold_ = false;
    if (in.follow_active && input.snap_rotation && out_of_dead_zone && in.mode != StereoMode::Screen &&
        !snap_latch_) {
        snap_time_ = last_snap_ = in.now;
        out.yaw                 = snapped_yaw(in.follow_yaw, in.turn, input.snap_rotation_segments);
        snap_latch_             = true;
    } else if ((!out_of_dead_zone && snap_latch_) ||
               (repeats && !snap_hold_ && in.follow_active && out_of_dead_zone && snap_latch_ &&
                in.now - snap_time_ > repeat)) {
        // Stick released, or held long enough to repeat.
        snap_latch_ = false;
        snap_time_  = {};
    }
}

// Spec 22 §4.2: the yaw follows the turn value at a rate in degrees per
// second, the same at any frame rate. Under the same conditions as a snap
// step. The first update after it did not apply turns by nothing, and no
// update turns for longer than kSmoothTurnMaxStep.
bool StereoUpdate::smooth_turn(const UpdateInput& in, UpdateOutput& out, float speed) noexcept {
    if (!in.follow_active || in.mode == StereoMode::Screen) return false;
    const auto step     = smooth_time_ ? std::min<std::chrono::steady_clock::duration>(in.now - *smooth_time_,
                                                                                       kSmoothTurnMaxStep)
                                       : std::chrono::steady_clock::duration::zero();
    smooth_time_        = in.now;
    const float seconds = std::chrono::duration<float>(step).count();
    const float yaw     = smoothed_yaw(in.follow_yaw, in.turn, speed, seconds);
    if (yaw != in.follow_yaw) out.yaw = yaw;
    return true;
}

// 05 §3.2e: recenter on the chord (or our own request), then re-place the
// menu screen for kScreenReplaceFrames updates. Requests wait until someone
// is wearing the headset upright. The request is cleared before the thunk
// takes the pose: a recenter that meets an invalid head pose queues the
// retake again.
void StereoUpdate::recenter(const UpdateInput& in, UpdateOutput& out) noexcept {
    const bool chord = (in.lb && in.rb) || (in.ctrl && in.alt);
    bool handled     = false;
    if (recenter_request_ && in.headset_in_use) {
        recenter_request_ = false;
        out.recenter      = true;
        recentered_       = true;
        screen_replaced_  = 0;
        handled           = true;
    } else if (chord) {
        if (!recentered_) {
            out.recenter = true;
            recentered_  = true;
            handled      = true;
        } else if (in.r3 && !r3_latch_) {
            r3_latch_ = true;
            handled   = true;
        }
    }
    if (handled) return;
    if (recentered_ && screen_replaced_ < kScreenReplaceFrames) {
        out.place_screen = true;
        ++screen_replaced_;
    } else if (!chord) {
        recentered_      = false;
        r3_latch_        = false;
        screen_replaced_ = 0;
    }
}

} // namespace omvr::core::stereo
