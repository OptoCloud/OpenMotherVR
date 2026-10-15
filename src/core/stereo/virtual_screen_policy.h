#pragma once

#include "core/diag/ports/log.h"
#include "core/stereo/ports/keyboard.h"
#include "core/stereo/ports/virtual_screen.h"
#include "core/stereo/types.h"
#include "core/vr/runtime_detector.h"

#include <mutex>

namespace omvr::core::stereo {

class StereoDevice;

// When the OpenXR virtual screen drives the frames (09 §8): only when OpenXR
// could not get a stereo device, so the game's finished frame goes on a
// virtual screen. (OpenVR runs through the stereo device.) Render thread,
// from the swapchain's Present.
class VirtualScreenPolicy final {
public:
    VirtualScreenPolicy(ports::VirtualScreen& screen, const ports::Keyboard& keyboard, StereoDevice& stereo,
                        vr::RuntimeDetector& runtimes, diag::ports::Log& log) noexcept
        : screen_(screen), keyboard_(keyboard), stereo_(stereo), runtimes_(runtimes), log_(log) {}

    // Called on every frame, before the real Present. Returns true when the
    // OpenXR virtual screen is pacing this frame (it has already blocked in
    // xrWaitFrame): the caller must then present with SyncInterval 0, or the
    // game waits for both the headset and the monitor and judders at the
    // slower of the two rates.
    bool on_present(SwapchainHandle swapchain) noexcept;

private:
    // Whether this frame is the virtual screen's at all.
    bool drives_frames() noexcept;
    // Starts the session on the first such frame, once.
    void start_once(SwapchainHandle swapchain) noexcept;

    ports::VirtualScreen& screen_;
    const ports::Keyboard& keyboard_;
    StereoDevice& stereo_;
    vr::RuntimeDetector& runtimes_;
    diag::ports::Log& log_;
    std::once_flag started_;
    bool recenter_held_ = false;
};

} // namespace omvr::core::stereo
