#pragma once

#include "core/input/virtual_pad.h"
#include "core/stereo/mode_policy.h"
#include "core/stereo/scene_tracker.h"
#include "game/input/input_manager.h"   // R: stereo (A3): an UpdateInput value struct
#include "game/stereo/follow_camera.h"  // R: stereo (A3): an UpdateInput value struct
#include "game/layout.h"

#include <chrono>

namespace omvr::core::stereo {

// Stereo::Update replaced: snap turn on the turn axis, recenter on LB+RB or
// Ctrl+Alt, game look only when snap is off or keyboard/mouse is used, scene
// visibility for cutscenes and loading. The game's own update (DK2
// recenter, fade) is not run. Game thread.
// From: FUN_1001f8a0.
class StereoUpdate {
public:
    StereoUpdate(input::VirtualPad& pad, game::input::InputManager& devices, game::stereo::FollowCamera& follow,
                 SceneTracker& scene, ModePolicy& modes) noexcept
        : pad_(pad), devices_(devices), follow_(follow), scene_(scene), modes_(modes) {}

    // One update of the manager `self` while its device runs stereo.
    void update_running(game::StereoManager* self, game::StereoInterface* device);

    // Recenter on the next stereo update: now, or once the headset is in use.
    // Used when gameplay starts (a loading screen closes) and at launch.
    void request_recenter() noexcept { g_update.recenter_request = true; }

    // The 3D scene goes black behind a loading screen.
    void set_loading_blackout(bool on) noexcept { g_update.loading_blackout = on; }

private:
    input::VirtualPad&          pad_;
    game::input::InputManager&  devices_;
    game::stereo::FollowCamera& follow_;
    SceneTracker&               scene_;
    ModePolicy&                 modes_;

    // Update state. Original: StereoManager +0x310..+0x320.
    struct {
        bool loading_blackout = false;  // +0x310
        bool keyboard_mouse   = false;  // +0x311  K&M in use: allow game look
        bool r3_latch         = false;  // +0x313
        bool snap_latch       = false;  // +0x314
        std::chrono::steady_clock::time_point snap_time{};  // +0x318 (clock() ms there)
        bool recentered       = false;  // +0x31C
        bool recenter_request = false;  // +0x31D  runtime asked (Oculus), or us:
                                        // gameplay starting, headset put on
        int  screen_replaced  = 0;      // +0x320
    } g_update;
    std::chrono::steady_clock::time_point g_last_snap{};  // kept past the stick's release
};

} // namespace omvr::core::stereo
