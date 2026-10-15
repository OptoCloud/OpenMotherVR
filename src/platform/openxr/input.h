#pragma once

// OpenXR controller input: grip poses, buttons and axes for both hands,
// normalized across controller families. This layer reports the
// physical controls; mapping them to game actions happens elsewhere.

#include "platform/openxr/xr_convert.h"

#include <array>
#include <vector>

namespace omvr::platform::openxr {

enum class Hand : int { Left = 0, Right = 1 };

// The original ships two control schemes: thumbstick controllers (Touch,
// Index, WMR) and touchpad controllers (Vive wands), where touchpad regions
// stand in for face buttons ("Interact R Touchpad Down", "Reload / Activate
// R Touchpad Up", "Radial Menu R Touchpad Center").
enum class ControllerFamily { Unknown, Thumbstick, Touchpad };

struct HandState {
    bool  active     = false; // controller present and bound
    bool  grip_valid = false; // grip pose tracked this frame
    Pose  grip{};             // seated space; the palm
    bool  grip_velocity_valid = false;
    Vec3  grip_velocity{};    // seated space, metres per second
    float trigger     = 0.0f; // 0..1
    float squeeze     = 0.0f; // grip button, 0..1
    Vec2  stick{};            // thumbstick, or touchpad on Vive wands
    bool  stick_click = false;
    bool  primary     = false; // A / X
    bool  secondary   = false; // B / Y
    bool  menu        = false;
};

struct ControllerState {
    std::array<HandState, 2> hands{};
    ControllerFamily         family = ControllerFamily::Unknown;

    const HandState& operator[](Hand h) const noexcept { return hands[static_cast<int>(h)]; }
};

class Input {
public:
    // Create the action set, suggest bindings for every supported profile and
    // attach it to the session. Must run before the session begins.
    bool create(XrInstance instance, XrSession session) noexcept;
    void destroy() noexcept;

    // Sync actions and locate the hands at `time` in `base`. Call once per frame
    // after xrBeginFrame, at the start of the frame (Session::frame()).
    void sync(XrTime time, XrSpace base) noexcept;

    // Re-query which controllers are bound (XR_TYPE_EVENT_DATA_INTERACTION_PROFILE_CHANGED).
    void on_interaction_profile_changed() noexcept;

    const ControllerState& state() const noexcept { return g_state; }

private:
    // One binding: action + component path under /user/hand/<side>/input or /output.
    struct Bind {
        XrAction*   action;
        const char* left;  // nullptr: not bound on that hand
        const char* right;
    };

    XrPath path(const char* s) noexcept;
    bool make_action(XrAction& out, XrActionType type, const char* name, const char* localized) noexcept;
    void suggest(const char* profile, const std::vector<Bind>& binds) noexcept;
    void suggest_all() noexcept;
    XrSpace make_action_space(XrAction action, XrPath hand) noexcept;
    XrActionStateGetInfo get_info(XrAction a, int hand) noexcept;
    float get_float(XrAction a, int hand, bool& active) noexcept;
    bool get_bool(XrAction a, int hand, bool& active) noexcept;
    Vec2 get_vec2(XrAction a, int hand, bool& active) noexcept;
    void locate_grip(int hand, XrSpace space, XrSpace base, XrTime t, HandState& s) noexcept;

    XrInstance  g_instance = XR_NULL_HANDLE;
    XrSession   g_session  = XR_NULL_HANDLE;
    XrActionSet g_set      = XR_NULL_HANDLE;

    std::array<XrPath, 2> g_hand_path{};

struct Actions {
    XrAction grip_pose   = XR_NULL_HANDLE;
    XrAction trigger     = XR_NULL_HANDLE;
    XrAction squeeze     = XR_NULL_HANDLE;
    XrAction stick       = XR_NULL_HANDLE;
    XrAction stick_click = XR_NULL_HANDLE;
    XrAction primary     = XR_NULL_HANDLE;
    XrAction secondary   = XR_NULL_HANDLE;
    XrAction menu        = XR_NULL_HANDLE;
} g_act;

    std::array<XrSpace, 2> g_grip_space{XR_NULL_HANDLE, XR_NULL_HANDLE};

    ControllerState g_state{};
};

} // namespace omvr::platform::openxr
