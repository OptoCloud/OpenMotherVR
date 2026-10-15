#pragma once

#include "core/diag/ports/log.h"
#include "core/input/types.h"
#include "core/vr/types.h"
#include "platform/openvr/binding_manifest.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <filesystem>

namespace vr {
class IVRInput;
} // namespace vr

// VR controller input through SteamVR Input (IVRInput): the action
// manifest and default bindings (binding_manifest.h), registered, and read
// each frame. Players can rebind everything in SteamVR's binding UI
// (04 §5.8.2); without IVRInput the legacy controller path applies
// (04 §5.8.3).
namespace omvr::platform::openvr {

class ApiLoader;

using Controls = core::input::Controls;

// A hand's grip pose as SteamVR reports it: runtime convention, unscaled.
struct HandPose {
    float m[3][4]{};     // row-major, m[r][3] = position, metres
    float velocity[3]{}; // metres per second
    bool valid = false;
};

// Render thread, after init.
class Actions final {
public:
    explicit Actions(core::diag::ports::Log& log) noexcept : log_(log) {}

    // Writes the manifest and bindings to `dir` and registers them. False
    // if SteamVR Input is unavailable (the caller keeps legacy input).
    bool init(ApiLoader& api, const std::filesystem::path& dir) noexcept;
    bool ready() const noexcept { return input_ != nullptr; }
    // Reads this frame's state. Call once per frame.
    Controls poll() noexcept;
    // Each hand's grip pose with the prediction of the last
    // IVRCompositor::WaitGetPoses (GetPoseActionDataForNextFrame), in the
    // compositor's tracking universe `origin` (vr::ETrackingUniverseOrigin).
    // Call on the thread that runs poll() and WaitGetPoses, after the latter.
    std::array<HandPose, core::vr::kHandCount> hand_poses(int origin) noexcept;

private:
    struct Analog {
        float x = 0, y = 0;
    };
    bool digital(Action a) noexcept;
    Analog analog(Action a) noexcept;
    bool write_files(const std::filesystem::path& dir, const std::filesystem::path& manifest_path);
    void register_actions(vr::IVRInput* input);
    void report_unbound_poses() noexcept;

    core::diag::ports::Log& log_;
    vr::IVRInput* input_ = nullptr;                                                // set once init() succeeds
    std::uint64_t set_   = 0;                                                      // VRActionSetHandle_t
    std::array<std::uint64_t, static_cast<std::size_t>(Action::Count)> handles_{}; // VRActionHandle_t
    std::array<std::uint64_t, core::vr::kHandCount> pose_handles_{};               // hand_left, hand_right
    bool unbound_logged_ = false;
};

} // namespace omvr::platform::openvr
