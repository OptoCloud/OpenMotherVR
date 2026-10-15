// Stereo::Update replaced. Analysis: dumps/06c_detours_and_patches.md
// (sections 3 and 4).

#include "core/stereo/stereo_update.h"

#include "core/config/settings_store.h"
#include "core/stereo/tuning.h"
#include "core/stereo/ui_placement.h"
#include "core/vr/ports/vr_runtime.h"
#include "game/input/input_devices.h"     // R: stereo (A3): an UpdateInput value struct
#include "game/stereo/stereo_objects.h"   // R: stereo (A3): an UpdateInput value struct

#include <algorithm>
#include <chrono>

namespace omvr::core::stereo {

namespace {

using game::input::kPadLB;
using game::input::kPadRB;
using game::input::kPadRStick;
using game::input::kScanLAlt;
using game::input::kScanLCtrl;
using game::stereo::field;
using game::stereo::kFollowLookEnabled;
using game::stereo::kFollowYaw;
using game::stereo::kFollowYawMirror;
using game::StereoInterface;
using game::StereoManager;

// The turn is the player map's analog action 3; its virtual code follows
// the map's digital count (0x12C on the shipped map), so it is derived.
constexpr int kTurnMap = 0, kTurnAction = 3;

} // namespace

void StereoUpdate::update_running(StereoManager* self, StereoInterface* device) {
    auto& u = g_update;
    const auto& input_cfg = config::current().input;
    void* pad      = devices_.pad_device();
    void* keyboard = devices_.keyboard_device();
    void* mouse    = devices_.mouse_device();

    bool lb = false, rb = false, r3 = false, ctrl = false, alt = false;
    if (pad) {
        lb = pad_.pad_button(pad, kPadLB);
        rb = pad_.pad_button(pad, kPadRB);
        r3 = pad_.pad_button(pad, kPadRStick);
        if (pad_.pad_activity(pad)) u.keyboard_mouse = false;

        const float turn = pad_.pad_axis_value(pad, pad_.analog_virtual_code(kTurnMap, kTurnAction));
        const bool  out  = turn < -kTurnThreshold || turn > kTurnThreshold;
        if (follow_.g_follow_active && input_cfg.snap_rotation && out && self->mode != 1 && !u.snap_latch) {
            // One step; yaw is in half turns, wrapped to [-1, 1].
            u.snap_time = g_last_snap = std::chrono::steady_clock::now();
            // A full turn is 2 half turns; in floating point, so 3 segments
            // step 120 degrees rather than 180.
            const float step = 2.0f / static_cast<float>(std::max(input_cfg.snap_rotation_segments, 2));
            float yaw = field<float>(follow_.g_follow, kFollowYaw) + (turn < -kTurnThreshold ? -step : step);
            if (yaw > 1.0f) yaw -= 2.0f;
            if (yaw < -1.0f) yaw += 2.0f;
            field<float>(follow_.g_follow, kFollowYaw) = yaw;
            field<float>(follow_.g_follow, kFollowYawMirror) = yaw;
            u.snap_latch = true;
        } else if ((!out && u.snap_latch) ||
                   (follow_.g_follow_active && out && u.snap_latch &&
                    std::chrono::steady_clock::now() - u.snap_time > kSnapRepeat)) {
            // Stick released, or held long enough to repeat.
            u.snap_latch = false;
            u.snap_time = {};
        }
    }
    if (keyboard && mouse) {
        ctrl = game::input::key_down(keyboard, kScanLCtrl);
        alt  = game::input::key_down(keyboard, kScanLAlt);
        if (game::input::any_key(keyboard) || game::input::mouse_active(mouse)) {
            u.keyboard_mouse = true;
        }
    }
    if (follow_.g_follow_active) {
        field<std::uint8_t>(follow_.g_follow, kFollowLookEnabled) = !input_cfg.snap_rotation || u.keyboard_mouse;
        follow_.trace_snap(g_last_snap, u.keyboard_mouse);
    }

    // Recenter on the chord (or our own request), then re-place the menu
    // screen for two frames. Requests wait until someone is wearing the
    // headset upright.
    using RecenterFn = float* (__fastcall*)(void*, void*, float*);
    auto recenter = [&] {
        reinterpret_cast<RecenterFn>(device->vtable[21])(device, nullptr, self->reference_pose);
    };
    const bool chord = (lb && rb) || (ctrl && alt);
    bool handled = false;
    vr::ports::VrRuntime* backend = game::load_backend(*device);
    if (u.recenter_request && backend && backend->headset_in_use()) {
        // Cleared first: a recenter that meets an invalid head pose queues
        // the retake again (dev_recenter_pose).
        u.recenter_request = false;
        recenter();
        u.recentered = true;
        u.screen_replaced = 0;
        handled = true;
    } else if (chord) {
        if (!u.recentered) {
            recenter();
            u.recentered = true;
            handled = true;
        } else if (r3 && !u.r3_latch) {
            u.r3_latch = true;
            handled = true;
        }
    }
    if (!handled) {
        if (u.recentered && u.screen_replaced <= 1) {
            place_virtual_screen(self->game_matrix, -1.0f, 0.5625f, -1.0f);
            ++u.screen_replaced;
        } else if (!chord) {
            u.recentered = false;
            u.r3_latch = false;
            u.screen_replaced = 0;
        }
    }

    modes_.sync_mode_stack(*self);

    // Scene visibility (+0x64): never black during cutscenes and fades,
    // black while a loading screen is up.
    if (scene_.g_cutscene_or_fade) {
        if (self->mode != 1) self->game_64 = 1.0f;
        if (!follow_.g_follow_active) {
            scene_.g_cutscene_took_camera = true;
        } else if (scene_.g_cutscene_took_camera) {
            scene_.g_cutscene_or_fade = scene_.g_cutscene_took_camera = false;
        }
    }
    if (u.loading_blackout) self->game_64 = 0.0f;
}

} // namespace omvr::core::stereo
