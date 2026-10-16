// OpenMotherVR-Provenance: clean
#pragma once

// The controllers as OpenXR reports them: grip poses, buttons and axes for
// both hands, normalised across controller families (04 §5.8.4). This layer
// reports the physical controls; mapping them to game actions happens
// elsewhere.

#include "core/vr/types.h"
#include "util/math/pose.h"

#include <array>
#include <cstddef>

namespace omvr::platform::openxr {

enum class Hand : int { Left = 0, Right = 1 };

// Two control schemes: thumbstick controllers (Touch, Index, WMR) and
// touchpad controllers (Vive wands), where touchpad regions stand in for
// face buttons (04 §5.5).
enum class ControllerFamily { Unknown, Thumbstick, Touchpad };

struct HandState {
    bool active     = false; // controller present and bound
    bool grip_valid = false; // grip pose tracked this frame
    util::Pose grip{};       // seated space; the palm
    bool grip_velocity_valid = false;
    util::Vec3 grip_velocity{}; // seated space, metres per second
    bool aim_valid = false;     // aim pose tracked this frame (only with the aim action, 21 §16)
    util::Pose aim{};           // seated space; /input/aim/pose
    float trigger = 0.0f;       // 0..1
    float squeeze = 0.0f;       // grip button, 0..1
    util::Vec2 stick{};         // thumbstick, or touchpad on Vive wands
    bool stick_click = false;
    bool primary     = false; // A / X
    bool secondary   = false; // B / Y
    bool menu        = false;
};

struct ControllerState {
    std::array<HandState, core::vr::kHandCount> hands{};
    ControllerFamily family = ControllerFamily::Unknown;
    // The session has input focus: the last xrSyncActions returned
    // XR_SUCCESS, not XR_SESSION_NOT_FOCUSED (a dashboard over the game).
    bool focused = false;

    const HandState& operator[](Hand h) const noexcept { return hands[static_cast<std::size_t>(h)]; }
};

} // namespace omvr::platform::openxr
