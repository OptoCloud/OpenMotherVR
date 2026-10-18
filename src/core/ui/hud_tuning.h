// OpenMotherVR-Provenance: clean
#pragma once

#include <chrono>
#include <numbers>

// The HUD options' constants (spec 23 §7, 22 §8.3).
namespace omvr::core::ui {

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
