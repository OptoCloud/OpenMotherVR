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

} // namespace omvr::core::hands
