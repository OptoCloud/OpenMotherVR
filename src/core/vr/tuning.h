#pragma once

#include <chrono>
#include <cstdint>

namespace omvr::core::vr {

// Start-up wait for the compositor and then a valid headset pose, in total
// (03 §6.4): a cold SteamVR start serves them some seconds after init.
inline constexpr std::uint64_t kStartupWaitMs = 10'000;

// Frames after the first submit at which the compositor is asked whether it
// shows this process: SteamVR has settled by then (about 3 s at 90 Hz).
inline constexpr std::uint32_t kSceneCheckFrame = 270;

// Below this horizontal length of the head's forward axis the heading is
// undefined (looking straight up or down); the previous yaw is kept
// (03 §2.1, 09 §5.5).
inline constexpr float kMinForwardLength = 1e-4f;

// The head's up axis must be within about 45 degrees of vertical for a
// recenter reference: the cosine (03 §2.1).
inline constexpr float kUprightCosine = 0.7f;

// The FOV tangent assumed until the runtime reports the real one.
inline constexpr float kDefaultTangent = 1.2f;
// Each eye's offset from the head, metres, until the runtime reports it:
// half an average IPD.
inline constexpr float kDefaultEyeX = 0.032f;

// Where the virtual screen goes when no pose is set: this far ahead of the
// seated origin and this wide, in metres.
inline constexpr float kDefaultScreenDistance = 2.0f;
inline constexpr float kDefaultScreenWidth    = 2.6f;

// A running session that the runtime never makes visible is reported this
// long after the session started.
inline constexpr auto kVisibleTimeout = std::chrono::seconds(10);

} // namespace omvr::core::vr
