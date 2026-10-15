// The one d3d11.dll export the proxy intercepts.

#include "core/diag/logger.h"
#include "loader/d3d11_forwarders.h"
#include "loader/hooks/crash_hook.h"
#include "loader/hooks/d3d11_hooks.h"

#include <windows.h>
#include <dxgi.h>

#include <mutex>

// ===== Exported forwarder ====================================================
// AI.exe's static import resolves here. Stays at file scope (no namespace)
// so the symbol name in the .def matches verbatim.

extern "C" __declspec(dllexport) HRESULT WINAPI D3D11CreateDeviceAndSwapChain(
    void* pAdapter, UINT DriverType, HMODULE Software, UINT Flags,
    const void* pFeatureLevels, UINT FeatureLevels, UINT SDKVersion,
    const void* pSwapChainDesc, void** ppSwapChain, void** ppDevice,
    void* pFeatureLevel, void** ppImmediateContext)
{
    omvr::loader::ensure_d3d11_loaded();
    namespace entry = omvr::loader::entry;
    // The attach-time message boxes (dialogs.h) are up until the player
    // closes them; as with the original, the game does not render before.
    entry::wait_for_dialogs();
    // The game's C runtime replaces the unhandled-exception filter during
    // its start-up, after attach and before WinMain; device creation is the
    // first call into us after that, and comes on every run (runtime None
    // has no Present hook). Once is enough here.
    static std::once_flag s_crash_filter;
    std::call_once(s_crash_filter, [] { omvr::loader::hooks::reassert_crash_hook(); });

    auto fn = entry::real_create_device_and_swapchain();
    if (!fn) {
        omvr::core::diag::log_line("proxy: forward attempted with NULL real ptr");
        return E_FAIL;
    }
    // Runtime None is the unmodded game: no swapchain change and no wrapper
    // on the device or the swapchain (no shader fix, no window changes).
    if (entry::ui_set_only()) {
        return fn(pAdapter, DriverType, Software, Flags, pFeatureLevels, FeatureLevels, SDKVersion,
                  pSwapChainDesc, ppSwapChain, ppDevice, pFeatureLevel, ppImmediateContext);
    }
    // With a headset attached the back buffer is both eyes side by side at
    // the headset's resolution, far past any display mode: keep the game
    // windowed (the window is only a mirror).
    DXGI_SWAP_CHAIN_DESC desc{};
    if (pSwapChainDesc && entry::headset_attached()) {
        desc = *static_cast<const DXGI_SWAP_CHAIN_DESC*>(pSwapChainDesc);
        if (!desc.Windowed) {
            desc.Windowed = TRUE;
            pSwapChainDesc = &desc;
            omvr::core::diag::log_line("proxy: VR active, swapchain forced windowed");
        }
        if (entry::mirror_size(desc.BufferDesc.Width, desc.BufferDesc.Height)) {
            pSwapChainDesc = &desc;
            omvr::core::diag::logf("proxy: mirror back buffer {}x{}", desc.BufferDesc.Width, desc.BufferDesc.Height);
        }
    }
    HRESULT hr = fn(pAdapter, DriverType, Software, Flags,
                    pFeatureLevels, FeatureLevels, SDKVersion,
                    pSwapChainDesc, ppSwapChain, ppDevice,
                    pFeatureLevel, ppImmediateContext);
    if (SUCCEEDED(hr) && ppDevice && *ppDevice) {
        omvr::loader::hooks::install_device_hooks(*ppDevice);
    }
    if (SUCCEEDED(hr) && ppSwapChain && *ppSwapChain) {
        omvr::loader::hooks::install_swapchain_hooks(*ppSwapChain);
    }
    return hr;
}
