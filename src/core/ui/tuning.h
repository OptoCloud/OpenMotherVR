#pragma once

#include <chrono>
#include <numbers>

namespace omvr::core::ui {

// The lock the processes of one game launch share while one builds
// MOTHER.PAK, and how long another waits for it (spec 14 §5.8): well above
// a rebuild, and short enough that a stuck process does not hold up the
// game's start noticeably.
inline constexpr const wchar_t* kArchiveLockName = L"Local\\MotherVR.MotherPak";
inline constexpr std::chrono::milliseconds kArchiveLockWait{5000};

// Runtime error codes the failure message tells apart (the runtimes' own
// values): SteamVR's VRInitError_Init_AnotherAppLaunching, which its
// Desktop Game Theatre causes, and XR_ERROR_RUNTIME_UNAVAILABLE, no
// loadable OpenXR runtime.
inline constexpr int kOpenVrAnotherAppLaunching = 117;
inline constexpr int kOpenXrRuntimeUnavailable  = -51;

// The HUD options (spec 23 §7, 22 §8.3).
//
// 23 §7.4, Follow placement: the panel stays put until the head's heading
// differs from it by more than this (normal glances stay below it), then
// moves to the head's heading over kHudFollowMove with an ease-in-out
// (fast enough to stay in view when turning, slow enough not to drag the
// eyes).
inline constexpr float kHudFollowThreshold = 20.0f * std::numbers::pi_v<float> / 180.0f;
inline constexpr std::chrono::milliseconds kHudFollowMove{300};

// Degrees to radians, for Hud.Pitch.
inline constexpr float kRadiansPerDegree = std::numbers::pi_v<float> / 180.0f;

} // namespace omvr::core::ui
