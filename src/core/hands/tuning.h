// OpenMotherVR-Provenance: clean
#pragma once

#include "util/time/steady.h"

#include <cstddef>

// Behaviour parameters of the hand and body features (spec 21), with the
// section each one meets. Values marked [proposal] in the spec are the spec's.
namespace omvr::core::hands {

using util::Seconds;

// 21 §3.3 [proposal]: a lost hand keeps its last pose relative to the head
// this long (covers brief occlusions without a jump), then blends to the
// game's own pose over kBlendOut; a hand that is back blends in over
// kBlendIn.
inline constexpr Seconds kLostHold{0.3f};
inline constexpr Seconds kBlendOut{0.3f};
inline constexpr Seconds kBlendIn{0.15f};

// 21 §3.2 [proposal]: the smoothing filter's minimum cut-off goes from
// 10 Hz at Hands.Smoothing 0 to 1 Hz at 1.
inline constexpr float kSmoothingCutoffAtZeroHz = 10.0f;
inline constexpr float kSmoothingCutoffAtOneHz  = 1.0f;
// The adaptive part of the filter (the public "1 Euro" filter): the cut-off
// rises by this much per unit of speed, so a moving hand does not lag, and
// the speed itself is filtered at kSpeedCutoffHz. Implementation choices
// (the spec leaves them open): 1 Hz per m/s and 0.5 Hz per rad/s take the
// cut-off past 10 Hz at an ordinary reach (about 2 m/s, 4 rad/s), the
// filter's published default for the speed is 1 Hz.
inline constexpr float kSmoothingBetaPerMetrePerSecond  = 1.0f;
inline constexpr float kSmoothingBetaPerRadianPerSecond = 0.5f;
inline constexpr float kSpeedCutoffHz                   = 1.0f;

// A gap between two updates longer than this (a menu, a loading screen, a
// hitch) restarts the filter and the tracking timers instead of filtering
// or blending across it.
inline constexpr Seconds kMaxUpdateGap{0.25f};

// The hands seen by the game thread's consumers (aim, tracker) are dropped
// when the last update is older than this: no head pose was asked for, so
// the game is not in first-person play.
inline constexpr Seconds kStaleAfter{0.25f};

// 21 §6.2: below this horizontal length of the hand's aim direction (it
// points nearly straight up or down) the tracker keeps its previous facing.
inline constexpr float kTrackerMinHorizontal = 0.1f;

// 21 §5.2: the distance from the hand's aim pose to the muzzle or nozzle,
// along the aim direction, per class. Not measured yet (21 §22 item 5); an
// offset along the ray moves where it starts, not where it points, so 0
// keeps the ray on the barrel's line meanwhile.
inline constexpr float kBarrelOffsetMetres = 0.0f;
inline constexpr float kNozzleOffsetMetres = 0.0f;

// 21 §5.2 (r11): the weapon transform's (WeaponTransform, 21 §2.5) column
// that is the barrel's axis, and the distance from its origin to the muzzle
// or nozzle along it, per class. Which column is the barrel and the offsets
// are to be read in game (21 §21 item 5): the column is the one the game's
// own camera and poses look along (local +Z, 21 §2.4), and 0 keeps the ray
// on the barrel's line meanwhile. Used only while the weapon is drawn in
// the hand the arm edit moved (21 §4.2 item 6).
inline constexpr int kWeaponBarrelColumn            = 2;
inline constexpr float kTransformBarrelOffsetMetres = 0.0f;
inline constexpr float kTransformNozzleOffsetMetres = 0.0f;
// A barrel column shorter than this (a degenerate transform) is not used.
inline constexpr float kMinBarrelColumnLength = 1.0e-3f;

// 21 §5.3: the fire alignment test passes when its length is above the
// threshold; a negative threshold passes every length the test computes.
inline constexpr float kFireGateAlwaysPass = -1.0f;

// ---- Melee and throwing by motion (21 §8, §9) ------------------------------------

// 21 §8.2 [proposal]: a swing is the right hand above kSwingStartMps, then
// below kSwingEndMps within kSwingWindow; it presses the fire action for
// kMotionPress, and swings within kSwingLockout of a press are ignored.
inline constexpr float kSwingStartMps = 2.0f;
inline constexpr float kSwingEndMps   = 1.0f;
inline constexpr Seconds kSwingWindow{0.5f};
inline constexpr Seconds kMotionPress{0.1f};
inline constexpr Seconds kSwingLockout{0.6f};
// 21 §8.2 (r11) [proposal]: from each start of the player's melee action
// the right hand blends out over kMeleeBlendOut, stays out until kMeleeHold
// after the start and blends back as returning tracking does (§3.3).
inline constexpr Seconds kMeleeBlendOut{0.05f};
inline constexpr Seconds kMeleeHold{0.9f};
inline constexpr Seconds kMeleeBlendBack = kBlendIn;
// A melee start this soon after a press by motion is that swing, for the
// log (21 §18; implementation choice: the press and the game's reaction to
// it fit well inside).
inline constexpr Seconds kMotionSwingAttribution{0.5f};

// 21 §9.2, §9.3 [proposal]: a release throws at a peak speed of at least
// kThrowReleaseMps over the kThrowVelocityWindow before it; the item should
// leave the hand within kThrowLaunchLimit of the release.
inline constexpr float kThrowReleaseMps = 1.5f;
inline constexpr Seconds kThrowVelocityWindow{0.05f};
inline constexpr Seconds kThrowLaunchLimit{0.15f};
// Velocity samples kept for the window: 50 ms at up to 640 head poses a
// second (implementation choice).
inline constexpr std::size_t kThrowSamples = 32;
// With Dev. Debug Options the game's velocity sets this long after a throw
// are logged, at most kThrowLaunchLines of them, to find the throw's launch
// (21 §22 item 8; implementation choice).
inline constexpr Seconds kThrowLaunchWatch{1.0f};
inline constexpr std::size_t kThrowLaunchLines = 4;

// 21 §12.1 (r11) [proposal]: a player action episode ends
// kActionCameraSettle after the first-person camera is back, or at the
// latest kActionSafety after its start.
inline constexpr Seconds kActionCameraSettle{0.5f};
inline constexpr Seconds kActionSafety{5.0f};

// ---- Haptics (21 §14) ----------------------------------------------------------

// 21 §14.2 [proposal]: each event's default amplitude (before
// Hands.Haptics.Strength) and duration, chosen so the events are told
// apart without masking each other.
// Damage: kDamageBase + kDamagePerLostMaximum * (health lost / maximum),
// capped at kHapticMaxAmplitude.
inline constexpr float kHapticMaxAmplitude   = 1.0f;
inline constexpr float kDamageBase           = 0.3f;
inline constexpr float kDamagePerLostMaximum = 2.0f;
inline constexpr Seconds kDamageDuration{0.15f};
inline constexpr float kDeathAmplitude = 1.0f;
inline constexpr Seconds kDeathDuration{0.6f};
// 21 §14.4 item 6: a tick per tracker scan that shows a contact, felt but
// faint.
inline constexpr float kTrackerPingAmplitude = 0.15f;
inline constexpr Seconds kTrackerPingDuration{0.02f};
// A shot; the flamethrower's softer pulse repeats while the fire control is
// held (a new one as each ends).
inline constexpr float kFiringAmplitude = 0.6f;
inline constexpr Seconds kFiringDuration{0.06f};
inline constexpr float kFlameAmplitude = 0.3f;
inline constexpr Seconds kFlameDuration{0.06f};
inline constexpr float kHackingAmplitude = 0.4f;
inline constexpr Seconds kHackingDuration{0.08f};

// 21 §14.3: the game's rumble is played as a series of short vibrations
// renewed each frame while its amplitude (§14.4) is not 0. Each lasts
// kRumbleRenewal: longer than a frame at any rate the headset runs (so
// the series has no gaps) and short enough that the vibration ends soon
// after the level drops (implementation choice).
inline constexpr Seconds kRumbleRenewal{0.05f};
// 21 §14.4 item 4 [proposal]: a level is held from one RumbleSetMotors call
// to the next, as the pad's motors hold their speed (the game sends a pair
// only when it changes, 21 §2.9), and dropped to 0 when no call came for
// this long: a safety against a level stuck on after the game's audio
// stops, for example at a pause.
inline constexpr Seconds kRumbleStale{0.5f};

// 21 §2.9: the game's pad curve in RumbleSetMotors, a level L (0 to 1) to a
// motor speed out of kMotorFullSpeed. At or below kPadDeadZone both motors
// stop. The low-frequency motor rises along one line up to kPadLowKnee and
// a steeper one above it; the high-frequency motor along one line.
inline constexpr float kMotorFullSpeed    = 65535.0f;
inline constexpr float kPadDeadZone       = 0.05f;
inline constexpr float kPadLowKnee        = 0.4f;
inline constexpr float kPadLowSlopeBelow  = 11468.626f;
inline constexpr float kPadLowOffsetBelow = 5242.8f;
inline constexpr float kPadLowSlopeAbove  = 75365.25f;
inline constexpr float kPadLowOffsetAbove = -9830.25f;
inline constexpr float kPadHighSlope      = 57343.0f;
inline constexpr float kPadHighOffset     = 8192.0f;
// 21 §14.4 item 3 [proposal]: an amplitude (the motor speed fraction
// squared, item 2) below this is not played: below 5 % of full output a VR
// actuator gives a buzz with no information.
inline constexpr float kRumbleAmplitudeFloor = 0.05f;

// 21 §14.2, §14.3: the vibration frequency asked of the runtimes with
// vibration actions: 0, the runtime's default (OpenXR spells it
// XR_FREQUENCY_UNSPECIFIED).
inline constexpr float kVibrationFrequencyDefault = 0.0f;
// 21 §14.3 [proposal]: OpenVR legacy input pulses each frame for
// amplitude * this many microseconds (the API's pulse is about 0 to 4 ms).
inline constexpr float kLegacyPulseMicrosecondsPerAmplitude = 3000.0f;

// ---- Body (21 §10 to §12) ------------------------------------------------------

// 21 §10.2 [proposal]: a hidden bone's scale. Not 0: skin bound to hidden
// bones only must shrink out of sight (a thousandth of its size is under a
// millimetre for a whole limb) while its normals stay finite, so a
// triangle shared with a drawn bone never shades from a zero normal.
inline constexpr float kCollapseScale = 1.0e-3f;

// 21 §10.2 (r8) [proposal]: a player skeleton that could not be read is
// read again this often until it can be, so a value set before the game
// has the player's skeleton ready (a menu, a level load) takes effect once
// it is; a read costs a walk of at most 256 bones.
inline constexpr Seconds kSkeletonRetry{1.0f};

// 21 §11.5 [proposal]: the character crouches when the head's height above
// the floor falls below kCrouchBelow * h and stands when it rises above
// kStandAbove * h (h: the calibrated standing eye height).
inline constexpr float kCrouchBelow = 0.78f;
inline constexpr float kStandAbove  = 0.86f;

// 21 §11.5 [proposal]: a press of the crouch action lasts this long, and
// the next comes no sooner than kCrouchRetry after it.
inline constexpr Seconds kCrouchPress{0.1f};
inline constexpr Seconds kCrouchRetry{0.5f};
// 21 §11.5: a press the game's requested crouch byte has not followed
// within this time did not take (logged once; no retry of a crouch press).
inline constexpr Seconds kCrouchConfirm{1.0f};
// 21 §12.2 item 3 [proposal]: the crouch is re-synced this long after an
// episode ends.
inline constexpr Seconds kHandBackSettle{0.5f};

// 21 §2.2, §11.5 [likely]: how far the game lowers the first-person camera
// when the player crouches, game metres: the difference of the game's
// `first_person_camera_offset_y` (0.01) and
// `first_person_camera_crouch_offset_y` (-0.62) in DATA\GLOBALCONSTANTS.TXT.
inline constexpr float kGameCrouchEyeDropM = 0.63f;
// 21 §11.5 [proposal]: the eye compensation follows the game's crouch over
// this time, so the view does not jump when the game's camera moves (the
// game's own transition time is not known, 21 §22 item 14).
inline constexpr Seconds kCrouchEyeBlend{0.3f};

} // namespace omvr::core::hands
