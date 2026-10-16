#pragma once

#include "core/config/settings.h"
#include "core/stereo/scene_tracker.h"
#include "core/stereo/turn_block.h"
#include "core/stereo/types.h"

#include <chrono>
#include <optional>

namespace omvr::core::stereo {

// What one stereo update reads, gathered by its thunk from the game's input
// devices, the active camera and the stereo manager.
struct UpdateInput {
    std::chrono::steady_clock::time_point now{};
    // The game has a pad device; the pad fields are read only then.
    bool pad          = false;
    bool lb           = false;
    bool rb           = false;
    bool r3           = false;
    bool pad_activity = false;
    float turn        = 0.0f; // the turn axis, -1..1
    // The merged movement value (player map analog actions 0 and 1, VR and
    // real pad), for the comfort vignette (spec 22 §5).
    float move_x = 0.0f;
    float move_y = 0.0f;
    // The game has a keyboard and a mouse; their fields are read only then.
    bool keyboard_and_mouse    = false;
    bool ctrl                  = false;
    bool alt                   = false;
    bool key_or_mouse_activity = false;
    // A first-person camera is active, and its target yaw in half turns.
    bool follow_active = false;
    float follow_yaw   = 0.0f;
    StereoMode mode    = StereoMode::World;
    // The interaction state of the game's script events (11 §5.3, spec 22
    // §9.1), for interaction-aware turning (spec 22 §4.4).
    bool in_interaction                 = false;
    input::Interaction interaction_kind = input::Interaction::None;
    bool breathing_game                 = false;
    // Someone wears the headset upright (VrRuntime::headset_in_use). Asked
    // only while a recenter waits (recenter_pending).
    bool headset_in_use = false;
};

// What one stereo update does, in this order: the thunk writes the yaw and
// the look byte, takes the reference pose, re-places the menu screen, brings
// the mode stack in line and writes the scene visibility.
struct UpdateOutput {
    std::optional<float> yaw;         // snap or smooth turn: the camera's new yaw
    std::optional<bool> look_enabled; // the game's own look, while a camera is active
    bool recenter     = false;        // take the reference pose (device slot 21)
    bool place_screen = false;        // re-place the menu screen in front of the view
    std::optional<float> scene_visibility;
    // For the debug trace of a snap step.
    std::chrono::steady_clock::time_point last_snap{};
    bool keyboard_mouse = false;
    // Spec 22 §5: the comfort vignette's target strength for this frame.
    float vignette_target = 0.0f;
    // Spec 22 §4.3: a snap step was made with the fade on.
    bool snap_fade = false;
    // Spec 22 §16: set on the update the turning in use changes (INF), or
    // the interaction-aware block does (DBG).
    std::optional<TurningInUse> turning_changed;
    std::optional<TurnBlockChange> block_changed;
};

// Stereo::Update replaced (05 §3.2): snap or smooth turn on the turn axis
// (spec 22 §4.1), none during interactions when asked (§4.4), recenter on
// LB+RB or Ctrl+Alt, game look only when neither applies or keyboard/mouse
// is used, scene visibility for cutscenes and loading, and the comfort
// vignette's target (§5). The game's own update
// (DK2 recenter, fade) is not run. Pure over the values its thunk reads.
// Game thread.
// From: FUN_1001f8a0.
class StereoUpdate final {
public:
    // `settings` is the live settings object: turning is read each update.
    StereoUpdate(const config::Settings& settings, SceneTracker& scene) noexcept
        : settings_(settings), scene_(scene) {}

    // One update while the manager's device runs stereo.
    UpdateOutput run(const UpdateInput& in) noexcept;

    // A recenter waits for the headset to be worn.
    bool recenter_pending() const noexcept { return recenter_request_; }

    // Recenter on the next stereo update: now, or once the headset is in use.
    // Used when gameplay starts (a loading screen closes), at launch, and
    // when a recenter met an invalid head pose.
    void request_recenter() noexcept { recenter_request_ = true; }

    // The 3D scene goes black behind a loading screen.
    void set_loading_blackout(bool on) noexcept { loading_blackout_ = on; }
    bool loading_blackout() const noexcept { return loading_blackout_; }

private:
    // Which turning applies (spec 22 §4.1); exactly one, chosen each update.
    Turning turning(const config::ComfortSettings& comfort) const noexcept;
    // Spec 22 §4.4: why turning is blocked now, None with the setting off;
    // a change is reported in `out`.
    TurnBlock block(const UpdateInput& in, UpdateOutput& out) noexcept;
    // While blocked: a held snap deflection neither steps nor repeats, and
    // must come back inside the threshold first once the block ends.
    void hold_snap(const UpdateInput& in) noexcept;
    void report_turning(Turning turn, const config::ComfortSettings& comfort, UpdateOutput& out) noexcept;
    // Spec 22 §5: moving with the stick, or the stick turning the view.
    static bool vignette_motion(const UpdateInput& in, Turning turn, TurnBlock blocked,
                                const UpdateOutput& out) noexcept;

    void snap_turn(const UpdateInput& in, UpdateOutput& out, const config::ComfortSettings& comfort) noexcept;
    // True when smooth turn applied this update.
    bool smooth_turn(const UpdateInput& in, UpdateOutput& out, float speed) noexcept;
    void recenter(const UpdateInput& in, UpdateOutput& out) noexcept;

    const config::Settings& settings_;
    SceneTracker& scene_;

    // Original: StereoManager +0x310..+0x320, noted per field.
    bool loading_blackout_ = false;                     // +0x310
    bool keyboard_mouse_   = false;                     // +0x311  K&M in use: allow game look
    bool r3_latch_         = false;                     // +0x313
    bool snap_latch_       = false;                     // +0x314
    bool snap_hold_        = false;                     // spec 22 §4.4: no repeat until released
    std::chrono::steady_clock::time_point snap_time_{}; // +0x318 (clock() ms there)
    bool recentered_       = false;                     // +0x31C
    bool recenter_request_ = false;                     // +0x31D  runtime asked (Oculus), or us:
                                                        // gameplay starting, headset put on
    int screen_replaced_ = 0;                           // +0x320
    std::chrono::steady_clock::time_point last_snap_{}; // kept past the stick's release
    // The previous update smooth turn applied in; empty when the last one
    // did not, so the first one turns by nothing (spec 22 §4.2).
    std::optional<std::chrono::steady_clock::time_point> smooth_time_;
    // The turning and the block last reported (spec 22 §16).
    std::optional<TurningInUse> reported_turning_;
    TurnBlockChange reported_block_{};
};

} // namespace omvr::core::stereo
