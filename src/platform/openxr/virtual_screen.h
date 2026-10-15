#pragma once

#include "core/stereo/ports/virtual_screen.h"

namespace omvr::platform::openxr {

class Session;

// The OpenXR session driven from the game's Present when there is no stereo
// device: the game's finished frame on a virtual screen.
class VirtualScreen final : public core::stereo::ports::VirtualScreen {
public:
    explicit VirtualScreen(Session& session) noexcept : session_(session) {}

    // The session is bound to the game's own device so the game's images can
    // be copied into runtime swapchains without sharing.
    std::optional<bool> start(core::stereo::SwapchainHandle swapchain) noexcept override;
    void recenter() noexcept override;
    // xrWaitFrame paces the game here; the real Present must not wait for
    // the monitor's vsync as well.
    bool frame(core::stereo::SwapchainHandle swapchain) noexcept override;

private:
    Session& session_;
};

} // namespace omvr::platform::openxr
