// Stereo::Update replaced (05 §3.2). Analysis:
// dumps/06c_detours_and_patches.md (sections 3 and 4).

#include "core/stereo/stereo_update.h"

#include "core/stereo/snap_turn.h"
#include "core/stereo/tuning.h"

namespace omvr::core::stereo {

UpdateOutput StereoUpdate::run(const UpdateInput& in) noexcept {
    UpdateOutput out;
    if (in.pad) {
        // 05 §3.2b: the pad in use switches game look off again.
        if (in.pad_activity) keyboard_mouse_ = false;
        snap_turn(in, out);
    }
    if (in.keyboard_and_mouse && in.key_or_mouse_activity) keyboard_mouse_ = true;
    if (in.follow_active) out.look_enabled = !settings_.input.snap_rotation || keyboard_mouse_;
    out.last_snap      = last_snap_;
    out.keyboard_mouse = keyboard_mouse_;

    recenter(in, out);

    // 05 §3.2g: never black during cutscenes and fades, black while a
    // loading screen is up.
    out.scene_visibility = scene_.update(in.follow_active, in.mode);
    if (loading_blackout_) out.scene_visibility = kSceneBlack;
    return out;
}

// 05 §3.2c: one step per push of the stick; a held stick repeats after
// kSnapRepeat. Not in the menu screen's mode, where the game has no camera
// to turn.
void StereoUpdate::snap_turn(const UpdateInput& in, UpdateOutput& out) noexcept {
    const auto& input = settings_.input;
    const bool  out_of_dead_zone = turn_out(in.turn);
    if (in.follow_active && input.snap_rotation && out_of_dead_zone && in.mode != StereoMode::Screen &&
        !snap_latch_) {
        snap_time_ = last_snap_ = in.now;
        out.yaw     = snapped_yaw(in.follow_yaw, in.turn, input.snap_rotation_segments);
        snap_latch_ = true;
    } else if ((!out_of_dead_zone && snap_latch_) ||
               (in.follow_active && out_of_dead_zone && snap_latch_ && in.now - snap_time_ > kSnapRepeat)) {
        // Stick released, or held long enough to repeat.
        snap_latch_ = false;
        snap_time_  = {};
    }
}

// 05 §3.2e: recenter on the chord (or our own request), then re-place the
// menu screen for kScreenReplaceFrames updates. Requests wait until someone
// is wearing the headset upright. The request is cleared before the thunk
// takes the pose: a recenter that meets an invalid head pose queues the
// retake again.
void StereoUpdate::recenter(const UpdateInput& in, UpdateOutput& out) noexcept {
    const bool chord   = (in.lb && in.rb) || (in.ctrl && in.alt);
    bool       handled = false;
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
