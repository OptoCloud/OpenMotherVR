// OpenMotherVR-Provenance: clean
// The IDXGISwapChain and ID3D11Device replacements of the d3d11.dll proxy
// (01 §1). D3D11CreateDeviceAndSwapChain returns the game's device and
// swapchain, whose CreatePixelShader, Present, SetFullscreenState and
// ResizeBuffers are replaced here. Render thread: the game creates shaders
// and presents there.

#include "loader/hooks/d3d11_hooks.h"

#include "core/render/constant_smoke.h"
#include "core/render/dither_fix.h"
#include "loader/context.h"
#include "loader/hooks/crash_hook.h"
#include "loader/hooks/render_sprite_hooks.h"
#include "loader/hooks/window_hooks.h"
#include "platform/d3d11/constants.h"
#include "platform/d3d11/swapchain.h"
#include "platform/win32/patching/memory.h"
#include "platform/win32/process_info.h"

#include <windows.h>
#include <dxgi.h>

#include <atomic>
#include <cstdint>
#include <string_view>
#include <type_traits>

namespace omvr::loader::hooks {

namespace {

namespace ports = core::diag::ports;
using platform::d3d11::kCreatePixelShaderVtblIndex;
using platform::d3d11::kPresentVtblIndex;
using platform::d3d11::kResizeBuffersVtblIndex;
using platform::d3d11::kSetFullscreenStateVtblIndex;

constexpr std::string_view kFirstPresentLog  = "present #1";
constexpr std::string_view kResizeLog        = "ResizeBuffers count={} {}x{} fmt={} flags=0x{:X}";
constexpr std::string_view kResizeMirrorLog  = "ResizeBuffers: mirror back buffer {}x{}";
constexpr std::string_view kPixelShaderLog   = "proxy: CreatePixelShader hooked (shader fixes)";
constexpr std::string_view kPresentHookedLog = "proxy: Present hooked, original=0x{:08X}";
constexpr std::string_view kResizeHookedLog  = "proxy: ResizeBuffers hooked, original=0x{:08X}";

// The vtable methods replaced, as the system declares them (__stdcall, this
// first).
using PresentFn            = HRESULT(__stdcall*)(void* self, UINT sync_interval, UINT flags);
using ResizeBuffersFn      = HRESULT(__stdcall*)(void* self, UINT buffer_count, UINT width, UINT height,
                                                 DXGI_FORMAT format, UINT swapchain_flags);
using SetFullscreenStateFn = HRESULT(__stdcall*)(void* self, BOOL fullscreen, void* target);
// ID3D11Device* this, bytecode, size, ID3D11ClassLinkage*, ID3D11PixelShader** out.
using CreatePixelShaderFn = HRESULT(__stdcall*)(void* self, const void* code, SIZE_T size, void* linkage,
                                                void** out);

// The system's method saved from a slot, as its type. The one cast from
// the untyped slot values.
template <class Fn>
Fn system_method(const std::atomic<void*>& saved) noexcept {
    return reinterpret_cast<Fn>(saved.load(std::memory_order_acquire));
}

// A replacement as the untyped value a vtable slot holds.
template <class Fn>
void* slot_value(Fn* fn) noexcept {
    return reinterpret_cast<void*>(fn);
}

// The vtable of a COM object: its first pointer.
void** vtable_of(void* object) noexcept { return *static_cast<void***>(object); }

bool is_game_swapchain(void* sc) noexcept {
    return sc && sc == context().targets.game_swapchain.load(std::memory_order_acquire);
}

// With a headset attached the back buffer is both eyes side by side, far
// past any display mode: refuse to go fullscreen (the swapchain is created
// windowed for the same reason). DXGI's own answer for "not now".
HRESULT __stdcall hook_set_fullscreen_state(void* self, BOOL fullscreen, void* target) noexcept {
    if (is_game_swapchain(self) && context().window.refuse_fullscreen(fullscreen != FALSE)) {
        return DXGI_ERROR_NOT_CURRENTLY_AVAILABLE;
    }
    return system_method<SetFullscreenStateFn>(context().targets.set_fullscreen)(self, fullscreen, target);
}

HRESULT __stdcall hook_present(void* self, UINT sync_interval, UINT flags) noexcept {
    auto& ctx          = context();
    const auto present = system_method<PresentFn>(ctx.targets.present);
    if (!is_game_swapchain(self)) return present(self, sync_interval, flags);

    const unsigned long n = ctx.targets.present_count.fetch_add(1, std::memory_order_relaxed) + 1;

    // The stereo device drives VR init (once) and per-frame head-pose update.
    // True when OpenXR's virtual screen already paced this frame (xrWaitFrame):
    // present without vsync then, or the game waits twice.
    const bool vr_paced =
        ctx.screen_policy.on_present(core::stereo::SwapchainHandle{reinterpret_cast<std::uintptr_t>(self)});

    // The first call confirms the hook fired. No periodic line: this is the
    // render thread, and a log write can stall it. The first frame is also
    // a point to put the crash filter back on top, if the game installed
    // its own after attach (11, the crash record).
    if (n == 1) {
        // The game presents on its render thread (11 §1.4).
        ctx.thread_roles.set(core::diag::ThreadRole::Render, platform::win32::current_thread_id());
        ctx.log.line(kFirstPresentLog);
        reassert_crash_hook();
    }
    return present(self, vr_paced ? 0 : sync_interval, flags);
}

HRESULT __stdcall hook_resize_buffers(void* self, UINT buffer_count, UINT width, UINT height,
                                      DXGI_FORMAT format, UINT swapchain_flags) noexcept {
    auto& ctx = context();
    if (is_game_swapchain(self)) {
        ports::logf(ctx.log, kResizeLog, buffer_count, width, height, static_cast<unsigned>(format),
                    static_cast<unsigned>(swapchain_flags));
        // Reset the frame counter so the next Present logs a fresh first
        // line, which ties resize events to what follows.
        ctx.targets.present_count.store(0, std::memory_order_relaxed);
        if (ctx.window.mirror_size(width, height)) ports::logf(ctx.log, kResizeMirrorLog, width, height);
    }
    const HRESULT hr = system_method<ResizeBuffersFn>(ctx.targets.resize_buffers)(
        self, buffer_count, width, height, format, swapchain_flags);
    if (SUCCEEDED(hr) && is_game_swapchain(self)) {
        auto* sc = static_cast<IDXGISwapChain*>(self);
        // The fit is posted to the window's procedure (spec 03 §9.3): a
        // headset attached since the swapchain was created subclasses it now.
        if (void* hwnd = platform::d3d11::output_window(sc)) keep_focus_while_vr(hwnd);
        platform::d3d11::fit_game_window(sc, ctx.window.headset_attached(), ctx.log);
    }
    return hr;
}

// ---- ID3D11Device::CreatePixelShader: shader fixes (08, 24 §15) -----------
// Also shared by every device of the class. Not limited to the game's: the
// patches only rewrite the game's own dither pattern and atlas sample, so
// another device's shaders pass through unchanged.

HRESULT __stdcall hook_create_pixel_shader(void* self, const void* code, SIZE_T size, void* linkage,
                                           void** out) noexcept {
    auto& ctx         = context();
    const auto create = system_method<CreatePixelShaderFn>(ctx.targets.create_pixel_shader);
    auto& stats       = ctx.shader_fixes;
    const int total   = stats.count_created();
    const auto dither = core::render::patch_position_dither(code, size);
    if (dither.blob.empty() && dither.has_constants) stats.count_unchanged(total);
    const void* fixed       = dither.blob.empty() ? code : dither.blob.data();
    const SIZE_T fixed_size = dither.blob.empty() ? size : dither.blob.size();

    // 24 §15: on top of the dither fix, which changes other instructions.
    if (const auto brightness = ctx.constant_smoke.brightness()) {
        auto& smoke_stats     = ctx.constant_smoke.stats();
        const int smoke_total = smoke_stats.count_created();
        const auto smoke      = core::render::patch_constant_smoke(fixed, fixed_size, *brightness);
        if (!smoke.empty()) {
            const HRESULT hr = create(self, smoke.data(), smoke.size(), linkage, out);
            if (SUCCEEDED(hr)) {
                smoke_stats.count_fixed(smoke_total);
                if (!dither.blob.empty()) stats.count_fixed(total);
                return hr;
            }
            smoke_stats.count_rejected(hr);
        }
    }
    if (!dither.blob.empty()) {
        const HRESULT hr = create(self, dither.blob.data(), dither.blob.size(), linkage, out);
        if (SUCCEEDED(hr)) {
            stats.count_fixed(total);
            return hr;
        }
        stats.count_rejected(hr);
    }
    return create(self, code, size, linkage, out);
}

static_assert(std::is_convertible_v<decltype(&hook_present), PresentFn>);
static_assert(std::is_convertible_v<decltype(&hook_resize_buffers), ResizeBuffersFn>);
static_assert(std::is_convertible_v<decltype(&hook_set_fullscreen_state), SetFullscreenStateFn>);
static_assert(std::is_convertible_v<decltype(&hook_create_pixel_shader), CreatePixelShaderFn>);

// Replaces vtable slot `index` of `vtable` with `replacement`, the system's
// method stored in `original` first. False when the slot could not be
// written.
bool replace_slot(void** vtable, std::size_t index, void* replacement, std::atomic<void*>& original,
                  const char* method, ports::Log& log) noexcept {
    return platform::win32::replace_pointer(&vtable[index], replacement, original, method, log);
}

std::uintptr_t address_of(const std::atomic<void*>& saved) noexcept {
    return reinterpret_cast<std::uintptr_t>(saved.load(std::memory_order_relaxed));
}

} // namespace

void install_device_hooks(void* device) noexcept {
    static std::atomic<bool> done{false};
    if (!device || done.exchange(true)) return;
    auto& ctx = context();
    // 24 §15.3: the keys as they are now apply to the whole session.
    const core::config::GraphicsSettings graphics = ctx.settings.current().graphics;
    ctx.constant_smoke.start(graphics.constant_smoke_lighting, graphics.constant_smoke_brightness);
    if (replace_slot(vtable_of(device), kCreatePixelShaderVtblIndex, slot_value(&hook_create_pixel_shader),
                     ctx.targets.create_pixel_shader, "CreatePixelShader", ctx.log)) {
        ctx.log.line(kPixelShaderLog);
    }
    install_vertex_shader_hook(vtable_of(device)); // 24 §14
}

void install_swapchain_hooks(void* swapchain) noexcept {
    if (!swapchain) return;
    auto& ctx = context();
    ctx.targets.game_swapchain.store(swapchain, std::memory_order_release);
    bool expected = false;
    if (!ctx.targets.swapchain_hooks_installed.compare_exchange_strong(expected, true)) return;

    auto* sc      = static_cast<IDXGISwapChain*>(swapchain);
    const bool vr = ctx.window.headset_attached();
    platform::d3d11::log_swapchain_desc(sc, ctx.log);
    // Subclassed first: a fit from a thread other than the window's is
    // posted to its procedure (spec 03 §9.3).
    void* hwnd = platform::d3d11::output_window(sc);
    if (hwnd) keep_focus_while_vr(hwnd);
    platform::d3d11::fit_game_window(sc, vr, ctx.log);
    if (hwnd) platform::d3d11::no_alt_enter_while_vr(sc, hwnd, ctx.window.headset_attached(), ctx.log);

    // Each replacement goes in on its own: one that cannot be installed is
    // logged and the others still are. The stereo frame depends on none of
    // them; without Present the OpenXR path without a stereo device is off,
    // without ResizeBuffers the back buffer follows the game's size after a
    // resize, without SetFullscreenState the fullscreen option may break the
    // mirror.
    void** vtbl = vtable_of(sc);
    if (replace_slot(vtbl, kPresentVtblIndex, slot_value(&hook_present), ctx.targets.present, "Present",
                     ctx.log)) {
        ports::logf(ctx.log, kPresentHookedLog, address_of(ctx.targets.present));
    }
    replace_slot(vtbl, kSetFullscreenStateVtblIndex, slot_value(&hook_set_fullscreen_state),
                 ctx.targets.set_fullscreen, "SetFullscreenState", ctx.log);
    if (replace_slot(vtbl, kResizeBuffersVtblIndex, slot_value(&hook_resize_buffers),
                     ctx.targets.resize_buffers, "ResizeBuffers", ctx.log)) {
        ports::logf(ctx.log, kResizeHookedLog, address_of(ctx.targets.resize_buffers));
    }
}

} // namespace omvr::loader::hooks
