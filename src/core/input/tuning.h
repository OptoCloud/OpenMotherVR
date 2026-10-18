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

// ---- Gestures (22 §8, §9). Distances in metres, runtime space before the
// world scale; head frame: x right, y up, z back (22 §8.1). ----

// 22 §8.1: after input focus returns no gesture fires for this long, so a
// hand held still while the dashboard closes fires nothing.
inline constexpr std::chrono::milliseconds kGestureFocusSettle{500};

// 22 §8.2: both hands this close to the head with both grips held this
// long recenters; at the face for most of a second does not happen in play.
inline constexpr float kRecenterHandDistance = 0.30f;
inline constexpr std::chrono::milliseconds kRecenterHold{750};

// 22 §8.4 (r11): the holster zones, starting values to tune in play. The
// shoulders: above the head's height minus kShoulderBelowHead and behind
// the head by more than kShoulderBehind. The left hip: below the head's
// height minus kHipBelowHead, left of centre by more than kHipLeftOf and
// in front of the head by at most kHipFrontMost. The chest: between
// kChestBelowTop and kChestBelowBottom below the head, within
// kChestHalfWidth of centre, and from kChestFrontMost in front of the head
// to kShoulderBehind behind it. The hands' ordinary hold, about 0.3 to 0.4 m
// in front of and 0.5 m below the head, and hands resting in the lap are
// outside every zone, so a grip there aims or raises the tracker as in v1.
inline constexpr float kShoulderBelowHead = 0.25f;
inline constexpr float kShoulderBehind    = 0.05f;
inline constexpr float kHipBelowHead      = 0.60f;
inline constexpr float kHipLeftOf         = 0.18f;
inline constexpr float kHipFrontMost      = 0.10f;
inline constexpr float kChestBelowTop     = 0.20f;
inline constexpr float kChestBelowBottom  = 0.45f;
inline constexpr float kChestHalfWidth    = 0.15f;
inline constexpr float kChestFrontMost    = 0.20f;

// 22 §9.2: a deliberate lean is well beyond kLeanStart, a breath or a nod
// stays within kLeanStop (the hysteresis avoids chatter); the lean grows
// in proportion to the offset up to kLeanFull, then is full.
inline constexpr float kLeanStart = 0.08f;
inline constexpr float kLeanStop  = 0.05f;
inline constexpr float kLeanFull  = 0.25f;

// 22 §9.3: the mouth point, below and in front of the head position, and
// "hand over mouth" (press within kMouthPress, release beyond
// kMouthRelease), not "hand near the face".
inline constexpr float kMouthBelowHead = 0.09f;
inline constexpr float kMouthInFront   = 0.07f;
inline constexpr float kMouthPress     = 0.12f;
inline constexpr float kMouthRelease   = 0.18f;

} // namespace omvr::core::input
