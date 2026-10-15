// From: FUN_10001720 / FUN_10001740 (the camera tracking).
// Analysis: dumps/06c_detours_and_patches.md.

#include "game/stereo/follow_camera.h"

#include "core/config/settings_store.h" // R: stereo (A3): the debug flag through the Log port
#include "core/diag/logger.h"           // R: stereo (A3): log through an injected Log&
#include "core/stereo/tuning.h"

#include <cmath>

namespace omvr::game::stereo {

namespace {

namespace config = core::config;
namespace diag = core::diag;
using core::stereo::kSnapTraceTime;

} // namespace

void FollowCamera::trace_snap(std::chrono::steady_clock::time_point step, bool keyboard_mouse) noexcept {
    if (!config::current().debug.debug || step == std::chrono::steady_clock::time_point{}) return;
    const auto since = std::chrono::steady_clock::now() - step;
    if (since > kSnapTraceTime) return;
    const float dx = field<float>(g_follow, kFollowHeadPos) - field<float>(g_follow, kFollowRootPos);
    const float dz = field<float>(g_follow, kFollowHeadPos + 8) - field<float>(g_follow, kFollowRootPos + 8);
    diag::logf("[diag] snap +{}ms: yaw {:.3f} target {:.3f}, head-root {:.3f} m ({:.3f}, {:.3f}), look {}, k&m {}",
               std::chrono::duration_cast<std::chrono::milliseconds>(since).count(),
               field<float>(g_follow, kFollowYawMirror), field<float>(g_follow, kFollowYaw),
               std::hypot(dx, dz), dx, dz, field<std::uint8_t>(g_follow, kFollowLookEnabled),
               keyboard_mouse);
}

} // namespace omvr::game::stereo
