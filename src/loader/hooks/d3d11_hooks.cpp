// d3d11.dll proxy: the IDXGISwapChain and ID3D11Device replacements.
//
// The original MotherVR is a dxgi.dll proxy. Windows 11 will not load a
// local dxgi.dll, but it does load a local d3d11.dll (see ../../README.md),
// so this rebuild proxies d3d11 instead. It gives the same intercept points:
// D3D11CreateDeviceAndSwapChain returns the game's device and swapchain,
// whose CreatePixelShader and Present we hook.

#include "loader/hooks/d3d11_hooks.h"

#include "core/render/dither_fix.h"
#include "loader/context.h"
#include "loader/hooks/crash_hook.h"
#include "loader/hooks/window_hooks.h"
#include "platform/d3d11/constants.h"
#include "platform/d3d11/swapchain.h"
#include "platform/win32/patching/memory.h"

#include <windows.h>
#include <dxgi.h>

#include <atomic>
#include <cstdint>
#include <type_traits>

namespace omvr::loader::hooks {

namespace {

namespace diag = core::diag;
using platform::d3d11::kCreatePixelShaderVtblIndex;
using platform::d3d11::kPresentVtblIndex;
using platform::d3d11::kResizeBuffersVtblIndex;
using platform::d3d11::kSetFullscreenStateVtblIndex;
using platform::win32::replace_pointer;

using PFN_Present = HRESULT (__stdcall*)(void* This, UINT SyncInterval, UINT Flags);
using PFN_ResizeBuffers = HRESULT (__stdcall*)(void* This, UINT BufferCount,
                                               UINT Width, UINT Height,
                                               DXGI_FORMAT NewFormat,
                                               UINT SwapChainFlags);
using PFN_SetFullscreenState = HRESULT (__stdcall*)(void* This, BOOL Fullscreen, void* Target);
// ID3D11Device* this, bytecode, size, ID3D11ClassLinkage*, ID3D11PixelShader** out.
using PFN_CreatePixelShader = HRESULT (__stdcall*)(void*, const void*, SIZE_T, void*, void**);

PFN_Present real_present() noexcept { return reinterpret_cast<PFN_Present>(context().originals.present); }
PFN_ResizeBuffers real_resize() noexcept {
    return reinterpret_cast<PFN_ResizeBuffers>(context().originals.resize_buffers);
}
PFN_SetFullscreenState real_set_fullscreen() noexcept {
    return reinterpret_cast<PFN_SetFullscreenState>(context().originals.set_fullscreen);
}
PFN_CreatePixelShader real_create_ps() noexcept {
    return reinterpret_cast<PFN_CreatePixelShader>(context().originals.create_pixel_shader);
}

bool is_game_swapchain(void* sc) noexcept {
    return sc && sc == context().originals.game_swapchain.load(std::memory_order_acquire);
}

// With a headset attached the back buffer is both eyes side by side, far
// past any display mode: refuse to go fullscreen (the swapchain is created
// windowed for the same reason). DXGI's own answer for "not now".
HRESULT __stdcall hook_SetFullscreenState(void* This, BOOL Fullscreen, void* Target) noexcept {
    if (is_game_swapchain(This) && context().window.refuse_fullscreen(Fullscreen != FALSE)) {
        return DXGI_ERROR_NOT_CURRENTLY_AVAILABLE;
    }
    return real_set_fullscreen()(This, Fullscreen, Target);
}
std::atomic<bool>  g_hooks_installed{false};
std::atomic<unsigned long> g_present_count{0};

HRESULT __stdcall hook_Present(void* This, UINT SyncInterval, UINT Flags) noexcept {
    if (!is_game_swapchain(This)) return real_present()(This, SyncInterval, Flags);

    const unsigned long n = g_present_count.fetch_add(1, std::memory_order_relaxed) + 1;

    // The stereo device drives VR init (once) and per-frame head-pose update.
    // True when OpenXR's virtual screen already paced this frame (xrWaitFrame):
    // present without vsync then, or the game waits twice.
    const bool vr_paced = context().screen_policy.on_present(
        core::stereo::SwapchainHandle{reinterpret_cast<std::uintptr_t>(This)});

    // The first call confirms the hook fired. No periodic line: this is the
    // render thread, and a log write can stall it. The first frame is also
    // a point to put the crash filter back on top, if the game installed
    // its own after attach (spec 11, the crash record).
    if (n == 1) {
        diag::log_line("present #1");
        reassert_crash_hook();
    }
    return real_present()(This, vr_paced ? 0 : SyncInterval, Flags);
}

HRESULT __stdcall hook_ResizeBuffers(void* This, UINT BufferCount,
                                     UINT Width, UINT Height,
                                     DXGI_FORMAT NewFormat,
                                     UINT SwapChainFlags) noexcept {
    if (is_game_swapchain(This)) {
        diag::logf("ResizeBuffers count={} {}x{} fmt={} flags=0x{:X}",
                   BufferCount, Width, Height,
                   static_cast<unsigned>(NewFormat),
                   static_cast<unsigned>(SwapChainFlags));
        // Reset frame counter so the next Present sample tags a "fresh" log
        // line, which helps correlate resize events with framerate changes.
        g_present_count.store(0, std::memory_order_relaxed);
        if (context().window.mirror_size(Width, Height))
            diag::logf("ResizeBuffers: mirror back buffer {}x{}", Width, Height);
    }
    const HRESULT hr = real_resize()(This, BufferCount, Width, Height, NewFormat, SwapChainFlags);
    if (SUCCEEDED(hr) && is_game_swapchain(This)) {
        platform::d3d11::fit_game_window(static_cast<IDXGISwapChain*>(This), context().window.headset_attached());
    }
    return hr;
}

// ---- ID3D11Device::CreatePixelShader: shader fixes ---------------------------
// Also shared by every device of the class. Not limited to the game's: the
// patch only rewrites the game's own dither pattern, so another device's
// shaders pass through unchanged.

HRESULT __stdcall hook_CreatePixelShader(void* self, const void* code, SIZE_T size,
                                         void* linkage, void** out) noexcept {
    auto& stats = context().shader_fixes;
    const int total = stats.count_created();
    int sites = 0;
    const auto patched = core::render::patch_position_dither(code, size, sites);
    if (!patched.empty()) {
        const HRESULT hr = real_create_ps()(self, patched.data(), patched.size(), linkage, out);
        if (SUCCEEDED(hr)) {
            stats.count_fixed(total);
            return hr;
        }
        stats.count_rejected(hr);
    }
    return real_create_ps()(self, code, size, linkage, out);
}

} // namespace

void install_device_hooks(void* device) noexcept {
    static std::atomic<bool> s_done{false};
    if (!device || s_done.exchange(true)) return;
    void** vtbl = *reinterpret_cast<void***>(device);
    if (auto* prev = replace_pointer(&vtbl[kCreatePixelShaderVtblIndex],
                                     reinterpret_cast<void*>(&hook_CreatePixelShader), "CreatePixelShader")) {
        context().originals.create_pixel_shader = prev;
        diag::log_line("proxy: CreatePixelShader hooked (shader fixes)");
    }
}

void install_swapchain_hooks(void* swapchain) noexcept {
    if (!swapchain) return;
    auto& ctx = context();
    ctx.originals.game_swapchain.store(swapchain, std::memory_order_release);
    bool expected = false;
    if (!g_hooks_installed.compare_exchange_strong(expected, true)) return;

    auto* sc = static_cast<IDXGISwapChain*>(swapchain);
    const bool vr = ctx.window.headset_attached();
    platform::d3d11::log_swapchain_desc(sc);
    platform::d3d11::fit_game_window(sc, vr);
    if (void* hwnd = platform::d3d11::output_window(sc)) {
        keep_focus_while_vr(hwnd);
        platform::d3d11::no_alt_enter_while_vr(sc, hwnd, ctx.window.headset_attached());
    }

    // Each replacement goes in on its own: one that cannot be installed is
    // logged and the others still are. The stereo frame depends on none of
    // them; without Present the OpenXR path without a stereo device is off,
    // without ResizeBuffers the back buffer follows the game's size after a
    // resize, without SetFullscreenState the fullscreen option may break the
    // mirror.
    void** vtbl = *reinterpret_cast<void***>(sc);

    if (auto* prev = replace_pointer(&vtbl[kPresentVtblIndex],
                                     reinterpret_cast<void*>(&hook_Present), "Present")) {
        ctx.originals.present = prev;
        diag::logf("proxy: Present hooked, original=0x{:08X}", reinterpret_cast<std::uintptr_t>(prev));
    }

    if (auto* prev = replace_pointer(&vtbl[kSetFullscreenStateVtblIndex],
                                     reinterpret_cast<void*>(&hook_SetFullscreenState), "SetFullscreenState")) {
        ctx.originals.set_fullscreen = prev;
    }

    if (auto* prev = replace_pointer(&vtbl[kResizeBuffersVtblIndex],
                                     reinterpret_cast<void*>(&hook_ResizeBuffers), "ResizeBuffers")) {
        ctx.originals.resize_buffers = prev;
        diag::logf("proxy: ResizeBuffers hooked, original=0x{:08X}", reinterpret_cast<std::uintptr_t>(prev));
    }
}

} // namespace omvr::loader::hooks
