// OpenMotherVR-Provenance: clean
// Per-frame Present: the OpenXR virtual-screen path (09 §8).

#include "core/stereo/virtual_screen_policy.h"

#include "core/stereo/stereo_device.h"

#include <string_view>

namespace omvr::core::stereo {

namespace {

constexpr std::string_view kStartingLog   = "[stereo] starting OpenXR session";
constexpr std::string_view kInitFailedLog = "[stereo] OpenXR init failed, running without VR";

} // namespace

bool VirtualScreenPolicy::drives_frames() noexcept {
    // Only when OpenXR could not get a stereo device: the game's frame on a
    // virtual screen. With a stereo device the runtime drives the frames.
    if (runtimes_.last_result().chosen != vr::DetectResult::Chosen::OpenXR) return false;
    if (stereo_.device_attached()) return false;
    // The stereo device gave up on the runtime and the player was told VR
    // is off; do not start it again here for the virtual screen.
    return !stereo_.runtime_failed();
}

// 09 §8: the session is bound to the game's own device so the game's images can be
// copied into runtime swapchains without sharing.
void VirtualScreenPolicy::start_once(SwapchainHandle swapchain) noexcept {
    std::call_once(started_, [this, swapchain]() {
        log_.line(kStartingLog);
        if (const auto started = screen_.start(swapchain)) {
            stereo_.set_vr_session_active(*started);
            if (!*started) log_.line(kInitFailedLog);
        }
    });
}

bool VirtualScreenPolicy::on_present(SwapchainHandle swapchain) noexcept {
    if (!drives_frames()) return false;
    start_once(swapchain);

    // 09 §8: Ctrl+Alt recenters on the press.
    const bool held = keyboard_.ctrl_and_alt_held();
    if (held && !recenter_held_) screen_.recenter();
    recenter_held_ = held;

    // xrWaitFrame paces the game here; the real Present must not wait for
    // the monitor's vsync as well.
    return screen_.frame(swapchain);
}

} // namespace omvr::core::stereo
