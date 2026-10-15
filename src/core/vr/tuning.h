#pragma once

#include <chrono>
#include <cstdint>

// Behaviour parameters of the VR runtimes: project decisions, with the spec
// section each one meets.
namespace omvr::core::vr {

// Start-up wait for the compositor and then a valid headset pose, in total
// (03 §6.4): a cold SteamVR start serves them some seconds after init.
inline constexpr std::chrono::milliseconds kStartupWait{10'000};
// Within that wait, how often the compositor interface is asked for again,
// and how long to pause after a pose wait that failed at once, so neither
// loop spins a core.
inline constexpr std::chrono::milliseconds kCompositorPollInterval{50};
inline constexpr std::chrono::milliseconds kPoseRetryInterval{10};

// Frames after the first submit at which the compositor is asked whether it
// shows this process: SteamVR has settled by then (about 3 s at 90 Hz).
inline constexpr std::uint32_t kSceneCheckFrame = 270;

// Below this horizontal length of the head's forward axis the heading is
// undefined (looking straight up or down); the previous yaw is kept
// (03 §2.1, 09 §5.5).
inline constexpr float kMinForwardLength = 1e-4f;

// The head's up axis must be within about 45 degrees of vertical for a
// recenter reference: the cosine (03 §2.3).
inline constexpr float kUprightCosine = 0.7f;

// The FOV tangent assumed until the runtime reports the real one (09 §4.2).
inline constexpr float kDefaultTangent = 1.2f;
// Each eye's offset from the head, metres, until the runtime reports it:
// half an average IPD (09 §5.4).
inline constexpr float kDefaultEyeX = 0.032f;

// Where the virtual screen goes when no pose is set: this far ahead of the
// seated origin and this wide, in metres (09 §8).
inline constexpr float kDefaultScreenDistance = 2.0f;
inline constexpr float kDefaultScreenWidth    = 2.6f;

// A screen frame shows eye 0's half of the stereo texture, its middle half
// vertically (03 §7, 09 §5.2): in normalised coordinates, and as quarters
// of the height in pixels (integer division).
inline constexpr float         kScreenLeftHalfU      = 0.5f;
inline constexpr float         kScreenTopV           = 0.25f;
inline constexpr float         kScreenBottomV        = 0.75f;
inline constexpr std::uint32_t kScreenHeightQuarters = 4;
inline constexpr std::uint32_t kScreenTopQuarters    = 1;
inline constexpr std::uint32_t kScreenBottomQuarters = 3;

// A running session that the runtime never makes visible is reported this
// long after the session started (09 §3.1).
inline constexpr auto kVisibleTimeout = std::chrono::seconds(10);

} // namespace omvr::core::vr
