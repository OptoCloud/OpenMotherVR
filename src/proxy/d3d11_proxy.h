#pragma once

#include <windows.h>

namespace mothervr::proxy::d3d11 {

// Load System32\d3d11.dll (SysWOW64 for this 32-bit process) and resolve
// D3D11CreateDeviceAndSwapChain, the one function AI.exe imports, which we
// intercept, plus the 50 exports we pass through verbatim for other modules
// (d3d11_exports.inc).
//
// Runs once, on the first call of any of our exports, not in DllMain:
// LoadLibrary under the loader lock is a deadlock hazard. Thread-safe;
// later calls return the first result at once.
//
// Returns true if LoadLibraryEx and GetProcAddress succeeded and the
// returned handle is not our own DLL (defensive, see implementation).
bool ensure_initialized() noexcept;

// Free the real d3d11.dll. Called from DLL_PROCESS_DETACH on FreeLibrary
// (not at process exit).
void shutdown() noexcept;

// Signature of the real D3D11CreateDeviceAndSwapChain. We don't include the
// d3d11.h headers because every parameter is a 4-byte pointer or UINT on x86,
// so calling-convention-wise the void* placeholders are equivalent.
using PFN_D3D11CreateDeviceAndSwapChain = HRESULT (WINAPI*)(
    void*  pAdapter,
    UINT   DriverType,
    HMODULE Software,
    UINT   Flags,
    const void* pFeatureLevels,
    UINT   FeatureLevels,
    UINT   SDKVersion,
    const void* pSwapChainDesc,
    void** ppSwapChain,
    void** ppDevice,
    void*  pFeatureLevel,
    void** ppImmediateContext);

// Pointer to the real System32 implementation. Null until
// ensure_initialized() succeeds.
extern PFN_D3D11CreateDeviceAndSwapChain real_create_device_and_swapchain;

// Wires up the proxy's swapchain hooks (Present, SetFullscreenState,
// ResizeBuffers) and logs a one-time GetDesc snapshot of the chain
// (resolution, format, buffer count). Invoked by the exported forwarder once
// D3D11CreateDeviceAndSwapChain succeeds. `swapchain` becomes the game's
// swapchain, the only one the hooks act on; the vtable is patched once.
void install_swapchain_hooks(void* swapchain) noexcept;

} // namespace mothervr::proxy::d3d11
