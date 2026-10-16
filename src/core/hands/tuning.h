// OpenMotherVR-Provenance: clean
#pragma once

#include <chrono>

// Behaviour parameters of the hand features (spec 21), with the section
// each one meets. Values marked [proposal] in the spec are the spec's.
namespace omvr::core::hands {

using Seconds = std::chrono::duration<float>;

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

// 21 §5.3: the fire alignment test passes when its length is above the
// threshold; a negative threshold passes every length the test computes.
inline constexpr float kFireGateAlwaysPass = -1.0f;

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
// A tick per tracker scan, felt but faint.
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

// 21 §14.3: the game's rumble is played while its level is above this, as a
// series of short vibrations renewed each frame. Each lasts
// kRumbleRenewal: longer than a frame at any rate the headset runs (so
// the series has no gaps) and short enough that the vibration ends soon
// after the level drops (implementation choice).
inline constexpr float kRumbleFloor = 0.02f;
inline constexpr Seconds kRumbleRenewal{0.05f};
// A rumble level the game has not set again for this long counts as 0
// (implementation choice: the game's own pad stop at 0xFE7698 writes the
// pad directly, not through RumbleSetMotors, 21 §2.9, so a level could
// otherwise be left on; whether the game renews a held level is open,
// 21 §22 item 9).
inline constexpr Seconds kRumbleStale{0.5f};

// 21 §14.2, §14.3: the vibration frequency asked of the runtimes with
// vibration actions: 0, the runtime's default (OpenXR spells it
// XR_FREQUENCY_UNSPECIFIED).
inline constexpr float kVibrationFrequencyDefault = 0.0f;
// 21 §14.3 [proposal]: OpenVR legacy input pulses each frame for
// amplitude * this many microseconds (the API's pulse is about 0 to 4 ms).
inline constexpr float kLegacyPulseMicrosecondsPerAmplitude = 3000.0f;

} // namespace omvr::core::hands
