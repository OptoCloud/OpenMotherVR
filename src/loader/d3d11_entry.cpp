// OpenMotherVR-Provenance: clean
// The one d3d11.dll export the proxy intercepts (01 §1.3).

#include "loader/d3d11_forwarders.h"
#include "loader/hooks/crash_hook.h"
#include "loader/hooks/d3d11_hooks.h"

#include <windows.h>
#include <dxgi.h>

#include <mutex>
#include <string_view>

namespace {

constexpr std::string_view kNoRealFunctionLog = "proxy: forward attempted with NULL real ptr";
constexpr std::string_view kForcedWindowedLog = "proxy: VR active, swapchain forced windowed";
constexpr std::string_view kMirrorBufferLog   = "proxy: mirror back buffer {}x{}";

// With a headset attached the back buffer is both eyes side by side at the
// headset's resolution, far past any display mode: keep the game windowed
// (the window is only a mirror) and size the buffer for the mirror. Returns
// the description to create the swapchain with: `desc` when it changed
// anything, else the game's own.
const void* vr_swapchain_desc(const void* game_desc, DXGI_SWAP_CHAIN_DESC& desc) noexcept {
    namespace entry = omvr::loader::entry;
    if (!game_desc || !entry::headset_attached()) return game_desc;
    const void* chosen = game_desc;
    desc               = *static_cast<const DXGI_SWAP_CHAIN_DESC*>(game_desc);
    if (!desc.Windowed) {
        desc.Windowed = TRUE;
        chosen        = &desc;
        entry::log().line(kForcedWindowedLog);
    }
    if (entry::mirror_size(desc.BufferDesc.Width, desc.BufferDesc.Height)) {
        chosen = &desc;
        omvr::core::diag::ports::logf(entry::log(), kMirrorBufferLog, desc.BufferDesc.Width,
                                      desc.BufferDesc.Height);
    }
    return chosen;
}

} // namespace

// ===== Exported forwarder ====================================================
// AI.exe's static import resolves here. Stays at file scope (no namespace)
// so the symbol name in the .def matches verbatim.

extern "C" __declspec(dllexport) HRESULT WINAPI D3D11CreateDeviceAndSwapChain(
    void* adapter, UINT driver_type, HMODULE software, UINT flags, const void* feature_levels,
    UINT feature_level_count, UINT sdk_version, const void* swapchain_desc, void** swapchain, void** device,
    void* feature_level, void** immediate_context) noexcept {
    omvr::loader::ensure_d3d11_loaded();
    namespace entry = omvr::loader::entry;
    // The attach-time message boxes (dialogs.h) are up until the player
    // closes them; the game does not render before (01 §2.4).
    entry::wait_for_dialogs();
    // The game's C runtime replaces the unhandled-exception filter during
    // its start-up, after attach and before WinMain; device creation is the
    // first call into us after that, and comes on every run (runtime None
    // has no Present hook). Once is enough here.
    static std::once_flag crash_filter_reasserted;
    try {
        std::call_once(crash_filter_reasserted, [] {
            omvr::loader::hooks::reassert_crash_hook();
            // The first point after attach outside the loader lock (22 §14).
            omvr::loader::hooks::start_log_flusher();
        });
    } catch (...) {
        // call_once only throws if the callable does, and it is noexcept.
    }

    auto real = entry::real_create_device_and_swapchain();
    if (!real) {
        entry::log().line(kNoRealFunctionLog);
        return E_FAIL;
    }
    // Runtime None is the unmodded game: no swapchain change and no wrapper
    // on the device or the swapchain (no shader fix, no window changes).
    if (entry::ui_set_only()) {
        return real(adapter, driver_type, software, flags, feature_levels, feature_level_count, sdk_version,
                    swapchain_desc, swapchain, device, feature_level, immediate_context);
    }
    DXGI_SWAP_CHAIN_DESC desc{};
    const void* chosen_desc = vr_swapchain_desc(swapchain_desc, desc);
    const HRESULT hr = real(adapter, driver_type, software, flags, feature_levels, feature_level_count,
                            sdk_version, chosen_desc, swapchain, device, feature_level, immediate_context);
    if (SUCCEEDED(hr) && device && *device) omvr::loader::hooks::install_device_hooks(*device);
    if (SUCCEEDED(hr) && swapchain && *swapchain) omvr::loader::hooks::install_swapchain_hooks(*swapchain);
    return hr;
}
