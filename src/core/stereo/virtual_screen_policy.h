#pragma once

#include "core/stereo/ports/keyboard.h"
#include "core/stereo/ports/virtual_screen.h"

#include <mutex>

namespace omvr::core::stereo {

class StereoDevice;

// When the OpenXR virtual screen drives the frames: only when OpenXR could
// not get a stereo device, so the game's finished frame goes on a virtual
// screen. (OpenVR runs through the stereo device.) Render thread, from the
// swapchain's Present.
class VirtualScreenPolicy {
public:
    VirtualScreenPolicy(ports::VirtualScreen& screen, const ports::Keyboard& keyboard, StereoDevice& stereo) noexcept
        : screen_(screen), keyboard_(keyboard), stereo_(stereo) {}

    // Called on every frame, before the real Present. Returns true when the
    // OpenXR virtual screen is pacing this frame (it has already blocked in
    // xrWaitFrame): the caller must then present with SyncInterval 0, or the
    // game waits for both the headset and the monitor and judders at the
    // slower of the two rates.
    bool on_present(SwapchainHandle swapchain) noexcept;

private:
    ports::VirtualScreen&  screen_;
    const ports::Keyboard& keyboard_;
    StereoDevice&          stereo_;
    std::once_flag         s_init;
    bool                   s_recenter_held = false;
};

} // namespace omvr::core::stereo
