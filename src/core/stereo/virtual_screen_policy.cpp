// Per-frame Present: the OpenXR virtual-screen path.

#include "core/stereo/virtual_screen_policy.h"

#include "core/diag/logger.h"
#include "core/stereo/stereo_device.h"
#include "core/vr/runtime_detector.h"

namespace omvr::core::stereo {

bool VirtualScreenPolicy::on_present(SwapchainHandle swapchain) noexcept {
    // Only when OpenXR could not get a stereo device: the game's frame on a
    // virtual screen. With a stereo device the runtime drives the frames.
    if (vr::last_result().chosen != vr::detect_result::Chosen::OpenXR) return false;
    if (stereo_.device_attached()) return false;
    // The stereo device gave up on the runtime and the player was told VR
    // is off; do not start it again here for the virtual screen.
    if (stereo_.runtime_failed()) return false;

    // The session is bound to the game's own device so the game's images can
    // be copied into runtime swapchains without sharing.
    std::call_once(s_init, [this, swapchain]() {
        diag::log_line("[stereo] starting OpenXR session");
        if (const auto started = screen_.start(swapchain)) {
            stereo_.set_vr_session_active(*started);
            if (!*started) diag::log_line("[stereo] OpenXR init failed, running without VR");
        }
    });

    // Ctrl+Alt recenters, as in the original (readme: "Recalibration").
    const bool held = keyboard_.ctrl_and_alt_held();
    if (held && !s_recenter_held) screen_.recenter();
    s_recenter_held = held;

    // xrWaitFrame paces the game here; the real Present must not wait for
    // the monitor's vsync as well.
    return screen_.frame(swapchain);
}

} // namespace omvr::core::stereo
