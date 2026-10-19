// OpenMotherVR-Provenance: clean
#pragma once

#include "core/input/types.h"

#include <string_view>

// The controller rules both VR backends apply to what they read, written
// once so OpenVR and OpenXR cannot drift apart: which scheme a controller
// picks, which mapping a scheme reads with, and the near-the-head test.
// Pure; render thread in use.
namespace omvr::core::input {

// 04 §5.5: the scheme the right hand's controller picks, from the id
// `source` reports (a SteamVR controller type, or an OpenXR interaction
// profile path). The sources differ on purpose: SteamVR Input gives an
// Index its own variant, OpenXR gives it the thumbstick one (its pause entry
// names the menu button). OpenVR's legacy path is always the touchpad.
[[nodiscard]] ControlScheme scheme_for(ControllerSource source, std::string_view id) noexcept;

// 04 §5.2, §5.3: the touchpad mapping for the touchpad scheme, the
// thumbstick mapping for every other one.
[[nodiscard]] Controls scheme_controls(ControlScheme scheme, const HandInput& left,
                                       const HandInput& right) noexcept;

// Which head position a backend writes into BodyPoses::head.
enum class HeadPosition {
    // This frame's pose, valid only when head_valid is set (OpenVR: device 0
    // of GetLastPoses, which can be stale or zeroed while tracking is lost).
    ThisFrame,
    // The last valid pose, held across tracking loss, so it is usable
    // whatever head_valid says (OpenXR: the located views, 09 §5.5).
    Held,
};

// 04 §5.10: the right hand is within kNearHead of the head, which turns
// Interact into the flashlight. The right hand's pose must be valid, and
// with HeadPosition::ThisFrame the head's too.
[[nodiscard]] bool right_hand_near_head(const BodyPoses& poses, HeadPosition head) noexcept;

} // namespace omvr::core::input
