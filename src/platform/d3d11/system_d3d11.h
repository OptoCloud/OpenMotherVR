#pragma once

#include <span>

struct HINSTANCE__;

namespace omvr::platform::d3d11 {

// Signature of the real D3D11CreateDeviceAndSwapChain. We don't include the
// d3d11.h headers because every parameter is a 4-byte pointer or UINT on x86,
// so calling-convention-wise the void* placeholders are equivalent.
using PFN_D3D11CreateDeviceAndSwapChain = long(__stdcall*)(
    void*  pAdapter,
    unsigned int DriverType,
    void*  Software,
    unsigned int Flags,
    const void* pFeatureLevels,
    unsigned int FeatureLevels,
    unsigned int SDKVersion,
    const void* pSwapChainDesc,
    void** ppSwapChain,
    void** ppDevice,
    void*  pFeatureLevel,
    void** ppImmediateContext);

// The real System32\d3d11.dll (SysWOW64 for this 32-bit process).
class SystemD3d11 {
public:
    // Load the real d3d11.dll and resolve D3D11CreateDeviceAndSwapChain, the
    // one function AI.exe imports, which we intercept, plus the exports we
    // pass through verbatim for other modules, into `forward` by `names`.
    //
    // LoadLibrary under the loader lock is a deadlock hazard, so this runs
    // on the first call of any export, not in DllMain.
    //
    // Returns true if LoadLibraryEx and GetProcAddress succeeded and the
    // returned handle is not our own DLL (defensive, see implementation).
    bool load(std::span<void*> forward, std::span<const char* const> names) noexcept;

    // Free the real d3d11.dll. Called from DLL_PROCESS_DETACH on FreeLibrary
    // (not at process exit).
    void shutdown() noexcept;

    // The real implementation. Null until load() succeeds.
    PFN_D3D11CreateDeviceAndSwapChain create_device_and_swapchain() const noexcept {
        return real_create_device_and_swapchain;
    }

private:
    HINSTANCE__* g_self        = nullptr; // HMODULE
    HINSTANCE__* g_real_module = nullptr; // HMODULE
    PFN_D3D11CreateDeviceAndSwapChain real_create_device_and_swapchain = nullptr;
};

} // namespace omvr::platform::d3d11
