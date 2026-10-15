// The OpenXR virtual-screen path, driven from the swapchain Present hook.

#include "platform/openxr/virtual_screen.h"

#include "platform/openxr/session.h"

#include <cstdint>

namespace omvr::platform::openxr {

namespace {

IDXGISwapChain* swapchain_of(core::stereo::SwapchainHandle h) noexcept {
    return reinterpret_cast<IDXGISwapChain*>(static_cast<std::uintptr_t>(h));
}

} // namespace

std::optional<bool> VirtualScreen::start(core::stereo::SwapchainHandle handle) noexcept {
    IDXGISwapChain* swapchain = swapchain_of(handle);
    ID3D11Device* device = nullptr;
    if (swapchain && SUCCEEDED(swapchain->GetDevice(__uuidof(ID3D11Device), reinterpret_cast<void**>(&device)))) {
        const auto si = session_.init(device);
        device->Release(); // the session holds its own reference
        return si.ok;
    }
    return std::nullopt;
}

void VirtualScreen::recenter() noexcept { session_.recenter(); }

bool VirtualScreen::frame(core::stereo::SwapchainHandle swapchain) noexcept {
    return session_.frame(swapchain_of(swapchain));
}

} // namespace omvr::platform::openxr
