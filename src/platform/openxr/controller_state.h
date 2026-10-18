// OpenMotherVR-Provenance: clean
#pragma once

// The controllers as OpenXR reports them: grip poses, buttons and axes for
// both hands, normalised across controller families (04 §5.8.4). This layer
// reports the physical controls; mapping them to game actions happens
// elsewhere.

#include "core/input/types.h"
#include "core/vr/types.h"
#include "util/math/pose.h"

#include <array>
#include <cstddef>
#include <optional>

namespace omvr::platform::openxr {

enum class Hand : int { Left = 0, Right = 1 };

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
    core::input::Dpad dpad{}; // a d-pad (the Steam Frame's left hand, 22 §7)
};

// XR_MAX_PATH_LENGTH (checked in input.cpp).
inline constexpr std::size_t kProfileNameSize = 256;

struct ControllerState {
    std::array<HandState, core::vr::kHandCount> hands{};
    // The right hand's control scheme, none while it has no profile:
    // Thumbstick (Touch, Index, WMR), Frame (the Steam Frame, thumbstick
    // controls with their own help) or Touchpad (Vive wands, where touchpad
    // regions stand in for face buttons, 04 §5.5).
    std::optional<core::input::ControlScheme> family;
    // Each hand's interaction profile path, empty when none is bound, read
    // at every profile change; the count of changes read (22 §10.1).
    std::array<std::array<char, kProfileNameSize>, core::vr::kHandCount> profiles{};
    unsigned profile_generation = 0;
    // The session has input focus: the last xrSyncActions returned
    // XR_SUCCESS, not XR_SESSION_NOT_FOCUSED (a dashboard over the game).
    bool focused = false;

    const HandState& operator[](Hand h) const noexcept { return hands[static_cast<std::size_t>(h)]; }
};

} // namespace omvr::platform::openxr
