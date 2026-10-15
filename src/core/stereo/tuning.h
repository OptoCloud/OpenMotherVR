#pragma once

#include <chrono>

namespace omvr::core::stereo {

// The virtual screen for menus, videos and loading: the left eye's image
// this far in front of where the head faced at the last recenter, and this
// wide (both metres).
inline constexpr float kCinemaDistance = 1.5f;
inline constexpr float kCinemaWidth    = 32.0f / 9.0f;

// A turn-axis value beyond this is a snap step.
inline constexpr float kTurnThreshold = 0.5f;
// A held stick repeats a snap step this often.
inline constexpr auto kSnapRepeat = std::chrono::milliseconds(300);
// Debug: how long after a snap step the camera is traced.
inline constexpr auto kSnapTraceTime = std::chrono::milliseconds(600);

// The head-in-wall fade counts as clear at or above this, and a fade that
// starts is held off this long in stereo.
inline constexpr float kFadeClear = 0.99999f;
inline constexpr auto  kFadeGrace = std::chrono::milliseconds(600);

} // namespace omvr::core::stereo
