// OpenMotherVR-Provenance: clean
#pragma once

// OpenXR controller input (04 §5.8.4): the action set, its suggested bindings
// per interaction profile, and the per-frame sync. Internal to this
// adapter: only its .cpp files include it.

#include "core/diag/ports/log.h"
#include "core/vr/ports/haptic_source.h"
#include "platform/openxr/controller_state.h"
#include "platform/openxr/xr_convert.h"
#include "platform/openxr/xr_handle.h"
#include "util/sync/change_latch.h"

#include <array>
#include <cstdint>
#include <vector>

namespace omvr::platform::openxr {

// Render thread (Session::frame and the session's creation).
class Input final {
public:
    explicit Input(core::diag::ports::Log& log) noexcept : log_(log) {}

    // Create the action set, suggest bindings for every supported profile and
    // attach it to the session. Must run before the session begins.
    bool create(XrInstance instance, XrSession session) noexcept;
    // With `on`, create() also makes the aim pose action with both hands'
    // subaction paths, suggested to /input/aim/pose (spec 21 §16), and sync
    // locates it. Before create().
    void set_aim_poses(bool on) noexcept { aim_poses_ = on; }
    // With `on`, create() also makes the vibration output action with both
    // hands' subaction paths, suggested to /output/haptic (spec 21 §14.3).
    // Before create().
    void set_haptics(bool on) noexcept { haptics_ = on; }
    void destroy() noexcept;

    // Sync actions and locate the hands at `time` in `base`. Call once per frame
    // after xrBeginFrame, at the start of the frame (Session::frame()).
    void sync(XrTime time, XrSpace base) noexcept;

    // Re-query which controllers are bound (XR_TYPE_EVENT_DATA_INTERACTION_PROFILE_CHANGED).
    void on_interaction_profile_changed() noexcept;

    const ControllerState& state() const noexcept { return state_; }

    // Plays this frame's vibration (21 §14.3): each hand's start or stop.
    // After sync(), on the same thread.
    void play_haptics(const core::vr::ports::HapticFrame& frame) noexcept;

private:
    // One binding: action + component path under /user/hand/<side>/input or /output.
    struct Bind {
        XrAction* action;
        const char* left; // nullptr: not bound on that hand
        const char* right;
    };

    struct Actions {
        XrAction grip_pose   = XR_NULL_HANDLE;
        XrAction trigger     = XR_NULL_HANDLE;
        XrAction squeeze     = XR_NULL_HANDLE;
        XrAction stick       = XR_NULL_HANDLE;
        XrAction stick_click = XR_NULL_HANDLE;
        XrAction primary     = XR_NULL_HANDLE;
        XrAction secondary   = XR_NULL_HANDLE;
        XrAction menu        = XR_NULL_HANDLE;
        XrAction aim_pose    = XR_NULL_HANDLE; // only with aim_poses_
        XrAction haptic      = XR_NULL_HANDLE; // only with haptics_
    };

    // Why a hand's grip was not located, logged once per hand and change.
    struct GripOutcome {
        bool locate_failed         = false;
        XrSpaceLocationFlags flags = 0;
        bool pose_active           = false;
        bool trigger_active        = false;

        friend bool operator==(const GripOutcome&, const GripOutcome&) = default;
    };

    XrPath path(const char* s) noexcept;
    bool make_action(XrAction& out, XrActionType type, const char* name, const char* localized) noexcept;
    bool make_actions() noexcept;
    void suggest(const char* profile, const std::vector<Bind>& binds) noexcept;
    void suggest_all() noexcept;
    XrSpace make_action_space(XrAction action, XrPath hand) noexcept;
    XrActionStateGetInfo get_info(XrAction a, int hand) noexcept;
    float get_float(XrAction a, int hand, bool& active) noexcept;
    bool get_bool(XrAction a, int hand, bool& active) noexcept;
    Vec2 get_vec2(XrAction a, int hand, bool& active) noexcept;
    HandState read_hand(int hand, XrSpace base, XrTime t) noexcept;
    void locate_grip(int hand, XrSpace space, XrSpace base, XrTime t, HandState& s) noexcept;
    void locate_aim(XrSpace space, XrSpace base, XrTime t, HandState& s) noexcept;
    void log_untracked_grip(int hand, XrResult located, XrSpaceLocationFlags flags) noexcept;
    void log_haptic_binding() noexcept;

    core::diag::ports::Log& log_;
    XrInstance instance_ = XR_NULL_HANDLE;
    XrSession session_   = XR_NULL_HANDLE;
    ActionSetOwner set_; // destroys its actions too

    std::array<XrPath, core::vr::kHandCount> hand_path_{};
    Actions actions_{};
    std::array<SpaceOwner, core::vr::kHandCount> grip_space_{};
    std::array<SpaceOwner, core::vr::kHandCount> aim_space_{};
    bool aim_poses_ = false;
    bool haptics_   = false;
    ControllerState state_{};

    // Logged on change; kept across destroy(), as the log is per process.
    std::array<util::ChangeLatch<GripOutcome>, core::vr::kHandCount> grip_logged_{};
    XrResult sync_logged_ = XR_SUCCESS;
};

} // namespace omvr::platform::openxr
