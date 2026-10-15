// The OpenXR virtual-screen path (09 §8), driven from the swapchain Present
// hook. Render thread.

#include "platform/openxr/virtual_screen.h"

#include "platform/d3d11/handles.h"
#include "platform/openxr/session.h"
#include "platform/openxr/session_impl.h"

namespace omvr::platform::openxr {

std::optional<bool> VirtualScreen::start(core::stereo::SwapchainHandle handle) noexcept {
    IDXGISwapChain* swapchain = d3d11::to_d3d(handle);
    ComPtr<ID3D11Device> device;
    if (!swapchain || FAILED(swapchain->GetDevice(IID_PPV_ARGS(device.GetAddressOf())))) return std::nullopt;
    // The session takes its own reference; ours goes at the end of the scope.
    return session_.init(d3d11::gpu_device(device.Get())).ok;
}

void VirtualScreen::recenter() noexcept { session_.recenter(); }

bool VirtualScreen::frame(core::stereo::SwapchainHandle swapchain) noexcept { return session_.frame(swapchain); }

} // namespace omvr::platform::openxr
