#pragma once

#include "input/vr_controls.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <filesystem>

namespace vr {
class IVRInput;
}

// VR controller input through SteamVR Input (IVRInput): an action manifest
// and default bindings per controller type, written next to the game.
// Replaces the original's legacy GetControllerState reading, which only fit
// Vive wands; players can rebind everything in SteamVR's binding UI.
//
// Not in the original (v1 fix: controls beyond the Vive wand).
namespace mothervr::runtime::openvr {

using Controls = input::Controls;

// The semantic game actions, in manifest order.
enum class Action : std::size_t {
    Move, Turn, TurnEngage, Sprint, Crouch, Fire, Aim, Tracker,
    Interact, Reload, Radial, Flashlight, Map, Pause,
    Count
};

// A hand's grip pose as SteamVR reports it: runtime convention, unscaled.
struct HandPose {
    float m[3][4]{};          // row-major, m[r][3] = position, metres
    float velocity[3]{};      // metres per second
    bool  valid = false;
};

class Actions {
public:
    // Writes the manifest and bindings to `dir` and registers them. False
    // if SteamVR Input is unavailable (the caller keeps legacy input).
    bool init(const std::filesystem::path& dir) noexcept;
    bool ready() const noexcept { return input_ != nullptr; }
    // Reads this frame's state. Call once per frame.
    Controls poll() noexcept;
    // Each hand's grip pose with the prediction of the last
    // IVRCompositor::WaitGetPoses (GetPoseActionDataForNextFrame), in the
    // compositor's tracking universe `origin` (vr::ETrackingUniverseOrigin).
    // Call on the thread that runs poll() and WaitGetPoses, after the latter.
    std::array<HandPose, 2> hand_poses(int origin) noexcept;

private:
    struct Analog {
        float x = 0, y = 0;
    };
    bool   digital(Action a) noexcept;
    Analog analog(Action a) noexcept;

    vr::IVRInput* input_ = nullptr;   // set once init() succeeds
    std::uint64_t set_   = 0;         // VRActionSetHandle_t
    std::array<std::uint64_t, static_cast<std::size_t>(Action::Count)> handles_{};  // VRActionHandle_t
    std::array<std::uint64_t, 2> pose_handles_{};  // hand_left, hand_right
    bool unbound_logged_ = false;
};

} // namespace mothervr::runtime::openvr
