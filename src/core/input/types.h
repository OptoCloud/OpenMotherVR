// OpenMotherVR-Provenance: clean
#pragma once

#include "util/math/vec.h"
#include "util/time/steady.h"

#include <array>
#include <cmath>

// VR controllers as the game sees them: one frame of semantic controls,
// whichever runtime read them (04 §5.8.1).
namespace omvr::core::input {

using util::TimePoint;
using util::Vec2;
using util::Vec3;

// The four directions of a d-pad, each pressed or not (22 §7). A real
// d-pad sets any of them; the emulation one at a time.
struct Dpad {
    bool up = false, down = false, left = false, right = false;

    [[nodiscard]] constexpr bool any() const noexcept { return up || down || left || right; }
    friend constexpr bool operator==(const Dpad&, const Dpad&) = default;
};

// The head and the hands as the runtime reported them for the frame its
// controls were read in (22 §8.1): positions in the runtime's tracking
// space (+Y up, -Z forward), metres before the world scale. A position
// whose flag is false is not used. Read-out for the gestures of 22 §8, §9.
struct BodyPoses {
    Vec3 head;
    float head_yaw  = 0;              // radians about +Y; 0 faces -Z
    bool head_valid = false;          // the head pose was valid
    std::array<Vec3, 2> hands{};      // grip positions, left then right
    std::array<bool, 2> hand_valid{}; // that hand's grip pose was valid
    bool focused = false;             // the runtime gives this app input focus (04 §5.1)
};

// The head yaw of BodyPoses from the head's forward (-Z) and up (+Y) axes in
// tracking space: the heading of the forward axis, from the up axis where
// the forward axis is near vertical (looking straight up or down), so the
// yaw stays defined at any pitch. Pure.
inline float head_yaw(const Vec3& forward, const Vec3& up) noexcept {
    const float x = forward.x - forward.y * up.x;
    const float z = forward.z - forward.y * up.z;
    return std::atan2(-x, -z);
}

// One frame of semantic controls (04 §5.8.1), written to the game's action
// tiers by the ControlsSink. Render thread.
struct Controls {
    Vec2 move;                // left stick / left touchpad (zero when untouched)
    Vec2 turn;                // right stick / right touchpad
    bool turn_engaged = true; // false while a touchpad is not clicked
    float trigger_l = 0, trigger_r = 0;
    bool grip_l = false, grip_r = false;
    bool sprint = false, interact = false, reload = false, radial = false;
    bool map = false, flashlight = false, pause = false;
    bool touchpad_scheme = false; // the touchpad scheme of 04 §5.2 (Vive wands)
    Dpad dpad;                    // a controller's own d-pad (04 §5.8.1, 22 §7)
    BodyPoses poses;              // the frame's head and hands (22 §8.1); none by default
};

// One hand's controller as a runtime reports it, before the scheme of
// 04 §5.2 or §5.3 gives its controls their meaning.
struct HandInput {
    Vec2 stick;                // thumbstick or touchpad position
    bool stick_touched = true; // a touchpad is touched (a stick always is)
    bool stick_click   = false;
    float trigger      = 0;
    bool grip          = false;
    bool primary       = false; // lower face button (A / X)
    bool secondary     = false; // upper face button (B / Y)
    bool menu          = false;
    Dpad dpad; // a d-pad on the controller (the Steam Frame's left hand)
};

// The touchpad regions of 04 §5.1: where the pad is pressed, and which
// virtual button a click there is (down interact, up reload, centre radial).
struct TouchpadRegions {
    bool horizontal = false;                               // |x| > |y|
    bool centre     = false;                               // inside the centre zone
    bool interact = false, reload = false, radial = false; // clicked regions
};

// The controller layout in use: picks the button-prompt glyphs (the
// touchpad set or the thumbstick set of 04 §4.1) and the controls help
// shown on the options page (04 §5.7).
// Frame: the Steam Frame controllers, thumbsticks with Map, Flashlight and
// Pause on their own buttons and a d-pad (04 §5.7).
enum class ControlScheme { Touchpad, Thumbstick, Index, Frame };

// How a runtime reads the controllers, which decides what identifies one
// and which default binding it has (04 §5.8, 22 §10.1).
enum class ControllerSource {
    SteamVrInput, // OpenVR with IVRInput: Prop_ControllerType_String
    OpenVrLegacy, // OpenVR without IVRInput: every controller is a wand
    OpenXr,       // the hand's interaction profile path
};

} // namespace omvr::core::input
