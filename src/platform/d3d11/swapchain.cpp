#include "platform/d3d11/swapchain.h"

#include "platform/win32/window/game_window.h"

#include <dxgi.h>
#include <wrl/client.h>

namespace omvr::platform::d3d11 {

namespace {

namespace ports = core::diag::ports;

constexpr std::string_view kGetDescFailedLog = "proxy: GetDesc failed";
constexpr std::string_view kSwapchainLog =
    "swapchain: {}x{} fmt={} buffers={} windowed={} usage=0x{:X} flags=0x{:X}";
constexpr std::string_view kAltEnterKeptLog =
    "proxy: DXGI Alt+Enter not switched off (hr=0x{:08X}); fullscreen requests are still refused";

} // namespace

void log_swapchain_desc(IDXGISwapChain* sc, ports::Log& log) noexcept {
    DXGI_SWAP_CHAIN_DESC desc{};
    if (FAILED(sc->GetDesc(&desc))) {
        log.line(kGetDescFailedLog);
        return;
    }
    ports::logf(log, kSwapchainLog, desc.BufferDesc.Width, desc.BufferDesc.Height,
                static_cast<unsigned>(desc.BufferDesc.Format), desc.BufferCount, desc.Windowed ? "yes" : "no",
                static_cast<unsigned>(desc.BufferUsage), static_cast<unsigned>(desc.Flags));
}

void* output_window(IDXGISwapChain* sc) noexcept {
    DXGI_SWAP_CHAIN_DESC desc{};
    return SUCCEEDED(sc->GetDesc(&desc)) ? desc.OutputWindow : nullptr;
}

void fit_game_window(IDXGISwapChain* sc, bool vr, ports::Log& log) noexcept {
    if (!vr) return;
    DXGI_SWAP_CHAIN_DESC desc{};
    if (SUCCEEDED(sc->GetDesc(&desc)) && desc.Windowed) win32::fit_window(desc.OutputWindow, log);
}

void no_alt_enter_while_vr(IDXGISwapChain* sc, void* window, bool vr, ports::Log& log) noexcept {
    const auto hwnd = static_cast<HWND>(window);
    if (!hwnd || !vr) return;
    Microsoft::WRL::ComPtr<IDXGIFactory> factory;
    HRESULT hr = sc->GetParent(IID_PPV_ARGS(factory.GetAddressOf()));
    if (SUCCEEDED(hr) && factory) hr = factory->MakeWindowAssociation(hwnd, DXGI_MWA_NO_ALT_ENTER);
    if (FAILED(hr)) ports::logf(log, kAltEnterKeptLog, static_cast<unsigned>(hr));
}

} // namespace omvr::platform::d3d11
