// d3d11.dll proxy + IDXGISwapChain::Present hook.
//
// The original MotherVR is a dxgi.dll proxy. Windows 11 will not load a
// local dxgi.dll, but it does load a local d3d11.dll (see ../../README.md),
// so this rebuild proxies d3d11 instead. It gives the same intercept points:
// D3D11CreateDeviceAndSwapChain returns the game's device and swapchain,
// whose CreatePixelShader and Present we hook.

#include "d3d11_proxy.h"
#include "../diag/log.h"
#include "../integration/dialogs.h"
#include "../stereo/present.h"
#include "../stereo/stereo_device.h"
#include "../render/shader_patch.h"
#include "../util/proc.h"

#include <atomic>
#include <cstdint>
#include <mutex>
#include <utility>
#include <dxgi.h>

// ===== Verbatim forwarders ===================================================
// One slot per pass-through export, filled by ensure_initialized() on the
// first call of any export. The thunks live at file scope (not in a
// namespace) so MSVC inline asm can name the table and the .def can alias
// each export to its mvr_fwd_* symbol.

#define MVR_D3D11_FORWARD(i, name) +1
constexpr unsigned kForwardCount = 0
#include "d3d11_exports.inc"
    ;
#undef MVR_D3D11_FORWARD

static void* g_forward[kForwardCount] = {};

// Nonzero once ensure_initialized() has run (whether or not it succeeded).
static volatile long g_forward_ready = 0;

#define MVR_D3D11_FORWARD(i, name) #name,
static const char* const kForwardNames[kForwardCount] = {
#include "d3d11_exports.inc"
};
#undef MVR_D3D11_FORWARD

extern "C" void __cdecl mvr_forward_init() {
    (void) mothervr::proxy::d3d11::ensure_initialized();
}

// Called by every thunk before it jumps. Once initialised it is a compare
// and a return. Otherwise it runs the one-time init with every general
// register saved, so the export's arguments (on the stack, or in ECX/EDX
// for a register convention) reach the real function untouched.
extern "C" __declspec(naked) void mvr_forward_ensure() {
    __asm {
        cmp g_forward_ready, 0
        jne ready
        pushad
        call mvr_forward_init
        popad
    ready:
        ret
    }
}

// Naked so the caller's stack frame reaches the real function untouched:
// signatures and calling conventions vary per export and don't matter here.
// The call to mvr_forward_ensure returns with the stack as it was.
#define MVR_D3D11_FORWARD(i, name)                                     \
    extern "C" __declspec(naked) void mvr_fwd_##name() {               \
        __asm call mvr_forward_ensure                                  \
        __asm jmp dword ptr [g_forward + (i) * 4]                      \
    }
#include "d3d11_exports.inc"
#undef MVR_D3D11_FORWARD

namespace mothervr::proxy::d3d11 {

namespace {

HMODULE g_self        = nullptr;
HMODULE g_real_module = nullptr;

// IDXGISwapChain vtable layout (from MSDN). The stereo path composes its
// frame in Present, and the eye buffers have to follow game resolution
// changes (ResizeBuffers).
//   0..2  IUnknown                QueryInterface, AddRef, Release
//   3..6  IDXGIObject             Set*/GetPrivateData, GetParent
//   7     IDXGIDeviceSubObject    GetDevice
//   8     IDXGISwapChain          Present                  <-- hooked
//   9                             GetBuffer
//   10                            SetFullscreenState       <-- hooked
//   11                            GetFullscreenState
//   12                            GetDesc                  <-- called once
//   13                            ResizeBuffers            <-- hooked
//   14                            ResizeTarget
//   ...
constexpr unsigned kPresentVtblIndex       = 8;
constexpr unsigned kResizeBuffersVtblIndex = 13;
constexpr unsigned kSetFullscreenStateVtblIndex = 10;

using PFN_Present = HRESULT (__stdcall*)(void* This, UINT SyncInterval, UINT Flags);
using PFN_ResizeBuffers = HRESULT (__stdcall*)(void* This, UINT BufferCount,
                                               UINT Width, UINT Height,
                                               DXGI_FORMAT NewFormat,
                                               UINT SwapChainFlags);

PFN_Present        g_real_present  = nullptr;
PFN_ResizeBuffers  g_real_resize   = nullptr;
using PFN_SetFullscreenState = HRESULT (__stdcall*)(void* This, BOOL Fullscreen, void* Target);
PFN_SetFullscreenState g_real_set_fullscreen = nullptr;

// The vtable slots are shared by every swapchain of the class in the process,
// including one an OpenXR runtime or an overlay creates in-process. The
// hooks act only on the game's: the last one created through our export.
std::atomic<void*> g_game_swapchain{nullptr};

bool is_game_swapchain(void* sc) noexcept {
    return sc && sc == g_game_swapchain.load(std::memory_order_acquire);
}

// With a headset attached the back buffer is both eyes side by side, far
// past any display mode: refuse to go fullscreen (the swapchain is created
// windowed for the same reason). DXGI's own answer for "not now".
HRESULT __stdcall hook_SetFullscreenState(void* This, BOOL Fullscreen, void* Target) {
    if (Fullscreen && is_game_swapchain(This) && mothervr::stereo::manager().device) {
        static bool s_logged = false;
        if (!std::exchange(s_logged, true)) {
            mothervr::diag::log_line("proxy: VR active, fullscreen refused");
        }
        return DXGI_ERROR_NOT_CURRENTLY_AVAILABLE;
    }
    return g_real_set_fullscreen(This, Fullscreen, Target);
}
std::atomic<bool>  g_hooks_installed{false};
std::atomic<unsigned long> g_present_count{0};

HRESULT __stdcall hook_Present(void* This, UINT SyncInterval, UINT Flags) {
    if (!is_game_swapchain(This)) return g_real_present(This, SyncInterval, Flags);

    const unsigned long n = g_present_count.fetch_add(1, std::memory_order_relaxed) + 1;

    // Stereo manager drives VR init (once) and per-frame head-pose update.
    // True when OpenXR's virtual screen already paced this frame (xrWaitFrame):
    // present without vsync then, or the game waits twice.
    const bool vr_paced = mothervr::stereo::on_present(static_cast<IDXGISwapChain*>(This));

    // The first call confirms the hook fired. No periodic line: this is the
    // render thread, and a log write can stall it.
    if (n == 1) mothervr::diag::log_line("present #1");
    return g_real_present(This, vr_paced ? 0 : SyncInterval, Flags);
}

// With a headset attached the game asks for a back buffer the size of both
// eyes (it takes its display mode from GetRenderTargetSize), but only the
// desktop mirror draws into it. Give it the window size the game asked for
// instead: the mirror reads the size from the buffer itself, and a stereo-
// sized buffer cost 0.3 ms GPU and 260 MB at 8064x4032.
bool mirror_size(UINT& width, UINT& height) noexcept {
    const auto* si = mothervr::stereo::load_device(mothervr::stereo::manager());
    if (!si || si->mirror_width <= 0 || si->mirror_height <= 0) return false;
    if (width == 0 || height == 0) return false; // "the window's size": already small
    if (width == static_cast<UINT>(si->mirror_width) && height == static_cast<UINT>(si->mirror_height))
        return false;
    width  = static_cast<UINT>(si->mirror_width);
    height = static_cast<UINT>(si->mirror_height);
    return true;
}

HRESULT __stdcall hook_ResizeBuffers(void* This, UINT BufferCount,
                                     UINT Width, UINT Height,
                                     DXGI_FORMAT NewFormat,
                                     UINT SwapChainFlags) {
    if (is_game_swapchain(This)) {
        mothervr::diag::logf("ResizeBuffers count={} {}x{} fmt={} flags=0x{:X}",
                             BufferCount, Width, Height,
                             static_cast<unsigned>(NewFormat),
                             static_cast<unsigned>(SwapChainFlags));
        // Reset frame counter so the next Present sample tags a "fresh" log
        // line, which helps correlate resize events with framerate changes.
        g_present_count.store(0, std::memory_order_relaxed);
        if (mirror_size(Width, Height))
            mothervr::diag::logf("ResizeBuffers: mirror back buffer {}x{}", Width, Height);
    }
    return g_real_resize(This, BufferCount, Width, Height, NewFormat, SwapChainFlags);
}

// VirtualProtect-swap a single vtable slot. Returns the previous slot value
// (so the caller can stash it as the "real" function pointer).
void* patch_vtable_slot(void** slot, void* replacement) noexcept {
    DWORD old_protect = 0;
    if (!::VirtualProtect(slot, sizeof(void*), PAGE_READWRITE, &old_protect)) {
        mothervr::diag::logf("proxy: VirtualProtect failed err=0x{:08X}",
                             ::GetLastError());
        return nullptr;
    }
    void* prev = *slot;
    *slot = replacement;
    DWORD ignore = 0;
    ::VirtualProtect(slot, sizeof(void*), old_protect, &ignore);
    return prev;
}

// ---- ID3D11Device::CreatePixelShader: shader fixes ---------------------------
// Also shared by every device of the class. Not limited to the game's: the
// patch only rewrites the game's own dither pattern, so another device's
// shaders pass through unchanged.

constexpr unsigned kCreatePixelShaderVtblIndex = 15;
// ID3D11Device* this, bytecode, size, ID3D11ClassLinkage*, ID3D11PixelShader** out.
using PFN_CreatePixelShader = HRESULT (__stdcall*)(void*, const void*, SIZE_T, void*, void**);
PFN_CreatePixelShader g_real_create_ps = nullptr;
std::atomic<int> g_ps_total{0}, g_ps_patched{0}, g_ps_rejected{0};

HRESULT __stdcall hook_CreatePixelShader(void* self, const void* code, SIZE_T size,
                                         void* linkage, void** out) {
    const int total = ++g_ps_total;
    int sites = 0;
    const auto patched = mothervr::render::patch_position_dither(code, size, sites);
    if (!patched.empty()) {
        const HRESULT hr = g_real_create_ps(self, patched.data(), patched.size(), linkage, out);
        if (SUCCEEDED(hr)) {
            const int n = ++g_ps_patched;
            if (n == 1 || n % 200 == 0) {
                mothervr::diag::logf("proxy: pixel shaders: {} created, {} dither-fixed, {} rejected",
                                     total, n, g_ps_rejected.load());
            }
            return hr;
        }
        if (++g_ps_rejected <= 3) {
            mothervr::diag::logf("proxy: patched pixel shader rejected (hr=0x{:08X}), using the original",
                                 static_cast<unsigned>(hr));
        }
    }
    return g_real_create_ps(self, code, size, linkage, out);
}

void install_device_hooks(void* device) noexcept {
    static std::atomic<bool> s_done{false};
    if (!device || s_done.exchange(true)) return;
    void** vtbl = *reinterpret_cast<void***>(device);
    if (auto* prev = patch_vtable_slot(&vtbl[kCreatePixelShaderVtblIndex],
                                       reinterpret_cast<void*>(&hook_CreatePixelShader))) {
        g_real_create_ps = reinterpret_cast<PFN_CreatePixelShader>(prev);
        mothervr::diag::log_line("proxy: CreatePixelShader hooked (shader fixes)");
    }
}

void log_swapchain_desc(IDXGISwapChain* sc) noexcept {
    DXGI_SWAP_CHAIN_DESC desc{};
    if (FAILED(sc->GetDesc(&desc))) {
        mothervr::diag::log_line("proxy: GetDesc failed");
        return;
    }
    mothervr::diag::logf(
        "swapchain: {}x{} fmt={} buffers={} windowed={} usage=0x{:X} flags=0x{:X}",
        desc.BufferDesc.Width, desc.BufferDesc.Height,
        static_cast<unsigned>(desc.BufferDesc.Format),
        desc.BufferCount,
        desc.Windowed ? "yes" : "no",
        static_cast<unsigned>(desc.BufferUsage),
        static_cast<unsigned>(desc.Flags));
}

bool load_real_d3d11() noexcept {
    ::GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                         reinterpret_cast<LPCWSTR>(&load_real_d3d11), &g_self);

    // Build absolute path to the real DLL. Passing just "d3d11.dll" to
    // LoadLibrary matches by basename in the loaded-modules list and would
    // return OUR handle, causing infinite recursion on the first forward.
    wchar_t sysdir[MAX_PATH];
    UINT sn = ::GetSystemDirectoryW(sysdir, MAX_PATH);
    if (sn == 0 || sn > MAX_PATH - 12) {
        diag::log_line("proxy: GetSystemDirectory failed");
        return false;
    }
    const wchar_t suffix[] = L"\\d3d11.dll";
    for (UINT i = 0; i < 11; ++i) sysdir[sn + i] = suffix[i];

    g_real_module = ::LoadLibraryExW(sysdir, nullptr, LOAD_WITH_ALTERED_SEARCH_PATH);
    if (!g_real_module) {
        diag::logf("proxy: LoadLibraryEx failed err=0x{:08X}",
                   ::GetLastError());
        return false;
    }
    if (g_real_module == g_self) {
        diag::log_line("proxy: LoadLibrary returned SELF, aborting");
        g_real_module = nullptr;
        return false;
    }
    diag::log_line("proxy: real d3d11 loaded");

    real_create_device_and_swapchain =
        util::proc<PFN_D3D11CreateDeviceAndSwapChain>(g_real_module, "D3D11CreateDeviceAndSwapChain");
    if (!real_create_device_and_swapchain ||
        real_create_device_and_swapchain ==
            util::proc<PFN_D3D11CreateDeviceAndSwapChain>(g_self, "D3D11CreateDeviceAndSwapChain")) {
        diag::log_line("proxy: GetProcAddress returned SELF or null, aborting");
        real_create_device_and_swapchain = nullptr;
        return false;
    }
    diag::log_line("proxy: D3D11CreateDeviceAndSwapChain resolved");

    // A missing name leaves its slot null; calling that export then faults
    // on the jmp, which is louder than any guessed fallback would be (we
    // can't fake a return without knowing the callee's stack cleanup).
    unsigned missing = 0;
    for (unsigned i = 0; i < kForwardCount; ++i) {
        g_forward[i] = util::proc<void*>(g_real_module, kForwardNames[i]);
        if (!g_forward[i]) {
            diag::logf("proxy: real d3d11 lacks export {}", kForwardNames[i]);
            ++missing;
        }
    }
    diag::logf("proxy: {} pass-through exports resolved, {} missing",
               kForwardCount - missing, missing);
    return true;
}

} // namespace

PFN_D3D11CreateDeviceAndSwapChain real_create_device_and_swapchain = nullptr;

bool ensure_initialized() noexcept {
    static std::once_flag s_once;
    static bool s_ok = false;
    try {
        std::call_once(s_once, [] {
            s_ok = load_real_d3d11();
            ::InterlockedExchange(&g_forward_ready, 1);
        });
    } catch (...) {
        // call_once only throws if the callable does, and it does not.
    }
    return s_ok;
}

void shutdown() noexcept {
    if (g_real_module) {
        ::FreeLibrary(g_real_module);
        g_real_module = nullptr;
    }
}

void install_swapchain_hooks(void* swapchain) noexcept {
    if (!swapchain) return;
    g_game_swapchain.store(swapchain, std::memory_order_release);
    bool expected = false;
    if (!g_hooks_installed.compare_exchange_strong(expected, true)) return;

    auto* sc = static_cast<IDXGISwapChain*>(swapchain);
    log_swapchain_desc(sc);

    void** vtbl = *reinterpret_cast<void***>(sc);

    if (auto* prev = patch_vtable_slot(&vtbl[kPresentVtblIndex],
                                       reinterpret_cast<void*>(&hook_Present))) {
        g_real_present = reinterpret_cast<PFN_Present>(prev);
        diag::logf("proxy: Present hooked, original=0x{:08X}",
                   reinterpret_cast<std::uintptr_t>(g_real_present));
    } else {
        // Could not patch Present: abort to avoid a partially-hooked state.
        g_hooks_installed.store(false);
        return;
    }

    if (auto* prev = patch_vtable_slot(&vtbl[kSetFullscreenStateVtblIndex],
                                       reinterpret_cast<void*>(&hook_SetFullscreenState))) {
        g_real_set_fullscreen = reinterpret_cast<PFN_SetFullscreenState>(prev);
    }

    if (auto* prev = patch_vtable_slot(&vtbl[kResizeBuffersVtblIndex],
                                       reinterpret_cast<void*>(&hook_ResizeBuffers))) {
        g_real_resize = reinterpret_cast<PFN_ResizeBuffers>(prev);
        diag::logf("proxy: ResizeBuffers hooked, original=0x{:08X}",
                   reinterpret_cast<std::uintptr_t>(g_real_resize));
    } else {
        diag::log_line("proxy: ResizeBuffers patch failed, resize events will be silent");
    }
}

} // namespace mothervr::proxy::d3d11

// ===== Exported forwarder ====================================================
// AI.exe's static import resolves here. Stays at file scope (no namespace)
// so the symbol name in the .def matches verbatim.

extern "C" __declspec(dllexport) HRESULT WINAPI D3D11CreateDeviceAndSwapChain(
    void* pAdapter, UINT DriverType, HMODULE Software, UINT Flags,
    const void* pFeatureLevels, UINT FeatureLevels, UINT SDKVersion,
    const void* pSwapChainDesc, void** ppSwapChain, void** ppDevice,
    void* pFeatureLevel, void** ppImmediateContext)
{
    mothervr::proxy::d3d11::ensure_initialized();
    // The attach-time message boxes (dialogs.h) are up until the player
    // closes them; as with the original, the game does not render before.
    mothervr::integration::wait_for_message_boxes();

    auto fn = mothervr::proxy::d3d11::real_create_device_and_swapchain;
    if (!fn) {
        mothervr::diag::log_line("proxy: forward attempted with NULL real ptr");
        return E_FAIL;
    }
    // With a headset attached the back buffer is both eyes side by side at
    // the headset's resolution, far past any display mode: keep the game
    // windowed (the window is only a mirror).
    DXGI_SWAP_CHAIN_DESC desc{};
    if (pSwapChainDesc && mothervr::stereo::manager().device) {
        desc = *static_cast<const DXGI_SWAP_CHAIN_DESC*>(pSwapChainDesc);
        if (!desc.Windowed) {
            desc.Windowed = TRUE;
            pSwapChainDesc = &desc;
            mothervr::diag::log_line("proxy: VR active, swapchain forced windowed");
        }
        if (mothervr::proxy::d3d11::mirror_size(desc.BufferDesc.Width, desc.BufferDesc.Height)) {
            pSwapChainDesc = &desc;
            mothervr::diag::logf("proxy: mirror back buffer {}x{}", desc.BufferDesc.Width, desc.BufferDesc.Height);
        }
    }
    HRESULT hr = fn(pAdapter, DriverType, Software, Flags,
                    pFeatureLevels, FeatureLevels, SDKVersion,
                    pSwapChainDesc, ppSwapChain, ppDevice,
                    pFeatureLevel, ppImmediateContext);
    if (SUCCEEDED(hr) && ppDevice && *ppDevice) {
        mothervr::proxy::d3d11::install_device_hooks(*ppDevice);
    }
    if (SUCCEEDED(hr) && ppSwapChain && *ppSwapChain) {
        mothervr::proxy::d3d11::install_swapchain_hooks(*ppSwapChain);
    }
    return hr;
}
