#include "platform/d3d11/swapchain.h"

#include "core/diag/logger.h" // R: proxy (A2): log through an injected Log&
#include "platform/win32/window/game_window.h"

#include <dxgi.h>

namespace omvr::platform::d3d11 {

namespace {

namespace diag = core::diag;

} // namespace

void log_swapchain_desc(IDXGISwapChain* sc) noexcept {
    DXGI_SWAP_CHAIN_DESC desc{};
    if (FAILED(sc->GetDesc(&desc))) {
        diag::log_line("proxy: GetDesc failed");
        return;
    }
    diag::logf(
        "swapchain: {}x{} fmt={} buffers={} windowed={} usage=0x{:X} flags=0x{:X}",
        desc.BufferDesc.Width, desc.BufferDesc.Height,
        static_cast<unsigned>(desc.BufferDesc.Format),
        desc.BufferCount,
        desc.Windowed ? "yes" : "no",
        static_cast<unsigned>(desc.BufferUsage),
        static_cast<unsigned>(desc.Flags));
}

void* output_window(IDXGISwapChain* sc) noexcept {
    DXGI_SWAP_CHAIN_DESC desc{};
    return SUCCEEDED(sc->GetDesc(&desc)) ? desc.OutputWindow : nullptr;
}

void fit_game_window(IDXGISwapChain* sc, bool vr) noexcept {
    if (!vr) return;
    DXGI_SWAP_CHAIN_DESC desc{};
    if (SUCCEEDED(sc->GetDesc(&desc)) && desc.Windowed) win32::fit_window(desc.OutputWindow);
}

void no_alt_enter_while_vr(IDXGISwapChain* sc, void* window, bool vr) noexcept {
    const auto hwnd = static_cast<HWND>(window);
    if (!hwnd || !vr) return;
    IDXGIFactory* factory = nullptr;
    HRESULT hr = sc->GetParent(__uuidof(IDXGIFactory), reinterpret_cast<void**>(&factory));
    if (SUCCEEDED(hr) && factory) {
        hr = factory->MakeWindowAssociation(hwnd, DXGI_MWA_NO_ALT_ENTER);
        factory->Release();
    }
    if (FAILED(hr)) {
        diag::logf("proxy: DXGI Alt+Enter not switched off (hr=0x{:08X}); fullscreen requests are still refused",
                   static_cast<unsigned>(hr));
    }
}

} // namespace omvr::platform::d3d11
