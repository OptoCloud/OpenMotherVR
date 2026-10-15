#pragma once

#include "core/diag/ports/log.h"

#include <chrono>
#include <cstddef>
#include <cstdint>

// The game's first-person camera behaviour (FollowBehavior) and the fields
// the stereo update reads and writes (05 §6.1, §3.2c, §3.2d).
namespace omvr::game::stereo {

// FollowBehavior fields. The look byte is game::kFollowLookEnabledOffset.
inline constexpr std::ptrdiff_t kFollowYaw       = 0x204; // +-1 = +-180 degrees
inline constexpr std::ptrdiff_t kFollowYawMirror = 0x1FC; // the yaw the camera has reached
inline constexpr std::ptrdiff_t kFollowRootPos   = 0x1C0; // character root, world x y z
inline constexpr std::ptrdiff_t kFollowHeadPos   = 0x1CC; // "Head1" bone, world x y z

// The active first-person camera, none while no FollowBehavior is active:
// the object may be freed once deactivated (a level unload), so it is
// forgotten then. Original: DAT_1006863c, DAT_10068638, which keep the last
// one. Game thread.
class FollowCamera final {
public:
    // FollowBehavior::OnActivate on `behaviour`, before the game's own.
    void on_activate(void* behaviour) noexcept;
    // FollowBehavior::OnDeactivate: inactive before the game's own runs, and
    // forgotten after it if it is the tracked one.
    void on_deactivate_begin() noexcept { active_ = false; }
    void on_deactivate_end(void* behaviour) noexcept;

    bool active() const noexcept { return active_; }

    // The target yaw, in half turns. Only while active().
    float yaw() const noexcept;
    // Sets the target yaw and the reached yaw, so the camera turns at once.
    // Only while active().
    void set_yaw(float yaw) noexcept;
    // The game's own stick and mouse look (05 §3.2d). Only while active().
    void set_look_enabled(bool enabled) noexcept;

    // Debug: the camera around a snap step, to tell the body catching up
    // with the new heading (the eye follows the head bone, which swings
    // around the root as the body turns) from game look turning as well.
    // Only while active(); logs nothing with debug options off.
    void trace_snap(core::diag::ports::Log& log, std::chrono::steady_clock::time_point now,
                    std::chrono::steady_clock::time_point step, bool keyboard_mouse) const noexcept;

private:
    void* object_ = nullptr;
    bool active_  = false;
};

} // namespace omvr::game::stereo
