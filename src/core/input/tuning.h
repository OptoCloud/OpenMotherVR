#pragma once

namespace omvr::core::input {

// A touchpad reading (Vive wands) inside this square counts as its centre.
inline constexpr float kPadDeadZone = 0.33f;
// The right hand this close to the head (metres) turns Interact into the
// flashlight.
inline constexpr float kNearHead = 0.2f;

// Touchpad turn and move scales: the touchpad reaches full speed short of
// its rim.
inline constexpr float kTurnScale = 0.85f;
inline constexpr float kMoveScale = 1.1f;
// An analog magnitude above this counts as VR activity.
inline constexpr float kActive = 0.85f;

} // namespace omvr::core::input
