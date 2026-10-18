#pragma once

#include <chrono>
#include <cstdint>

// Behaviour parameters of the VR controls (04 §5). Each value names its
// reason; the spec section says whether it is a game rule or a project
// decision.
namespace omvr::core::input {

// 04 §5.1: a touchpad reading (Vive wands) with |x| and |y| both below this
// is the pad's centre: the middle third in each axis.
inline constexpr float kTouchpadCentre = 0.33f;

// 04 §5.1, §5.10: the right hand within this distance of the head (metres,
// runtime space, before the world scale) turns Interact into the
// flashlight.
inline constexpr float kNearHead = 0.2f;

// 04 §5.2: touchpad scales. Movement reaches full speed a little inside the
// rim of the round pad; turning from the pad, which has no spring return,
// stays a little below the full rate of a stick at its stop.
inline constexpr float kTouchpadTurnScale = 0.85f;
inline constexpr float kTouchpadMoveScale = 1.1f;

// 04 §5.1: an analog value above this magnitude counts as VR activity, the
// game's own HasActivity threshold for sticks and triggers.
inline constexpr float kActivityThreshold = 0.85f;

// 04 §5.1: a trigger counts as pressed above the game's own trigger press
// threshold, and an analog grip (squeeze) by the same rule.
inline constexpr float kTriggerPressed = 0.5f;
inline constexpr float kSqueezePressed = 0.5f;

// 04 §5.1: a right stick deflected past half way up or down selects the next
// or previous weapon, so drift or a light touch never does.
inline constexpr float kWeaponStickDeflection = 0.5f;

// 04 §5.1: every analog value handed to the game stays within the range the
// game's own pad delivers.
inline constexpr float kAnalogLimit = 1.0f;

// 22 §7: the left stick presses a d-pad direction beyond this along that
// axis (the larger of the two), and releases it below kDpadRelease, so a
// stick held at the threshold does not chatter.
inline constexpr float kDpadPress   = 0.6f;
inline constexpr float kDpadRelease = 0.4f;

// 22 §7: the left stick click held this long is the d-pad modifier; a
// shorter click stays sprint.
inline constexpr std::chrono::milliseconds kDpadModifierHold{300};

// 04 §3.2 item 3: the flags an appended pad record gets, by kind of action
// (composite-reference bit clear).
inline constexpr std::uint8_t kAppendedFlagsDigital = 0;
inline constexpr std::uint8_t kAppendedFlagsAnalog  = 1;

} // namespace omvr::core::input
