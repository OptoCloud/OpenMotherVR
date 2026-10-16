// Stereo::Update replaced (05 §3.2). Analysis:
// dumps/06c_detours_and_patches.md (sections 3 and 4).

#include "core/stereo/stereo_update.h"

#include "core/stereo/snap_turn.h"
#include "core/stereo/tuning.h"

#include <algorithm>

namespace omvr::core::stereo {

UpdateOutput StereoUpdate::run(const UpdateInput& in) noexcept {
    UpdateOutput out;
    const config::ComfortSettings comfort = settings_.comfort;
    const Turning turn                    = turning(comfort);
    bool smoothed                         = false;
    if (in.pad) {
        // 05 §3.2b: the pad in use switches game look off again.
        if (in.pad_activity) keyboard_mouse_ = false;
        if (turn == Turning::Snap) snap_turn(in, out, comfort);
        if (turn == Turning::Smooth) smoothed = smooth_turn(in, out, comfort.smooth_turn_speed);
    }
    if (!smoothed) smooth_time_.reset();
    if (in.keyboard_and_mouse && in.key_or_mouse_activity) keyboard_mouse_ = true;
    // Spec 22 §4.1: the game's own stick look only when it is the turning,
    // so the stick never turns twice.
    if (in.follow_active) out.look_enabled = turn == Turning::GameLook || keyboard_mouse_;
    out.last_snap      = last_snap_;
    out.keyboard_mouse = keyboard_mouse_;

    recenter(in, out);

    // 05 §3.2g: never black during cutscenes and fades, black while a
    // loading screen is up.
    out.scene_visibility = scene_.update(in.follow_active, in.mode);
    if (loading_blackout_) out.scene_visibility = kSceneBlack;
    return out;
}

StereoUpdate::Turning StereoUpdate::turning(const config::ComfortSettings& comfort) const noexcept {
    if (settings_.input.snap_rotation) return Turning::Snap;
    return comfort.smooth_turn_speed > 0.0f ? Turning::Smooth : Turning::GameLook;
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
    if (in.follow_active && input.snap_rotation && out_of_dead_zone && in.mode != StereoMode::Screen &&
        !snap_latch_) {
        snap_time_ = last_snap_ = in.now;
        out.yaw                 = snapped_yaw(in.follow_yaw, in.turn, input.snap_rotation_segments);
        snap_latch_             = true;
    } else if ((!out_of_dead_zone && snap_latch_) || (repeats && in.follow_active && out_of_dead_zone &&
                                                      snap_latch_ && in.now - snap_time_ > repeat)) {
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
