#pragma once

#include "core/stereo/types.h"

#include <optional>

namespace omvr::core::stereo::ports {

// The OpenXR virtual screen for runs without a stereo device: the game's
// finished frame on a screen in the headset. Render thread.
class VirtualScreen {
public:
    // Starts the session on the swapchain's device. nullopt when the
    // swapchain gave no device (nothing started), else whether it started.
    virtual std::optional<bool> start(SwapchainHandle swapchain) noexcept = 0;
    // Re-anchors the screen in front of the head.
    virtual void recenter() noexcept = 0;
    // Ends the previous frame and begins the next; true when this paced the
    // frame (the caller presents without vsync then).
    virtual bool frame(SwapchainHandle swapchain) noexcept = 0;

protected:
    ~VirtualScreen() = default;
};

} // namespace omvr::core::stereo::ports
