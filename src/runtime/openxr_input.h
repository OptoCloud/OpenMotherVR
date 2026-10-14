#pragma once

// OpenXR controller input: grip poses, buttons and axes for both hands,
// normalized across controller families. This layer reports the
// physical controls; mapping them to game actions happens elsewhere.

#include "xr_platform.h"

#include <array>

namespace mothervr::runtime::openxr::input {

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

// Create the action set, suggest bindings for every supported profile and
// attach it to the session. Must run before the session begins.
bool create(XrInstance instance, XrSession session) noexcept;
void destroy() noexcept;

// Sync actions and locate the hands at `time` in `base`. Call once per frame
// after xrBeginFrame, at the start of the frame (openxr::frame()).
void sync(XrTime time, XrSpace base) noexcept;

// Re-query which controllers are bound (XR_TYPE_EVENT_DATA_INTERACTION_PROFILE_CHANGED).
void on_interaction_profile_changed() noexcept;

const ControllerState& state() noexcept;

} // namespace mothervr::runtime::openxr::input
