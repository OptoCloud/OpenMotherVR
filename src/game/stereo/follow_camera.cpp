// From: FUN_10001720 / FUN_10001740 (the camera tracking).
// Analysis: dumps/06c_detours_and_patches.md.

#include "game/stereo/follow_camera.h"

#include "core/stereo/tuning.h"
#include "game/constants.h"
#include "game/stereo/field.h"

#include <cmath>
#include <string_view>

namespace omvr::game::stereo {

namespace {

using std::chrono::steady_clock;

constexpr auto kLookEnabled = static_cast<std::ptrdiff_t>(kFollowLookEnabledOffset);
// The z of a world x y z position (floats).
constexpr auto kPosZ = static_cast<std::ptrdiff_t>(2 * sizeof(float));

constexpr std::string_view kSnapTraceLog =
    "[diag] snap +{}ms: yaw {:.3f} target {:.3f}, head-root {:.3f} m ({:.3f}, {:.3f}), look {}, k&m {}";

} // namespace

void FollowCamera::on_activate(void* behaviour) noexcept {
    object_ = behaviour;
    active_ = true;
}

void FollowCamera::on_deactivate_end(void* behaviour) noexcept {
    if (object_ == behaviour) object_ = nullptr;
}

float FollowCamera::yaw() const noexcept { return field<float>(object_, kFollowYaw); }

void FollowCamera::set_yaw(float yaw) noexcept {
    field<float>(object_, kFollowYaw)       = yaw;
    field<float>(object_, kFollowYawMirror) = yaw;
}

void FollowCamera::set_look_enabled(bool enabled) noexcept {
    field<std::uint8_t>(object_, kLookEnabled) = enabled ? 1 : 0;
}

void FollowCamera::trace_snap(core::diag::ports::Log& log, steady_clock::time_point now,
                              steady_clock::time_point step, bool keyboard_mouse) const noexcept {
    if (!log.debug_enabled() || step == steady_clock::time_point{}) return;
    const auto since = now - step;
    if (since > core::stereo::kSnapTraceTime) return;
    const float dx = field<float>(object_, kFollowHeadPos) - field<float>(object_, kFollowRootPos);
    const float dz = field<float>(object_, kFollowHeadPos + kPosZ) - field<float>(object_, kFollowRootPos + kPosZ);
    const auto since_ms = std::chrono::duration_cast<std::chrono::milliseconds>(since).count();
    core::diag::ports::logf(log, kSnapTraceLog, since_ms, field<float>(object_, kFollowYawMirror),
                            field<float>(object_, kFollowYaw), std::hypot(dx, dz), dx, dz,
                            field<std::uint8_t>(object_, kLookEnabled), keyboard_mouse);
}

} // namespace omvr::game::stereo
