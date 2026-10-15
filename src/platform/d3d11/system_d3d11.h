#pragma once

#include "core/diag/ports/log.h"

#include <span>

struct HINSTANCE__;

namespace omvr::platform::d3d11 {

// Signature of the real D3D11CreateDeviceAndSwapChain. No d3d11.h here:
// every parameter is a 4-byte pointer or UINT on x86, so the void*
// placeholders have the same calling convention and stack layout.
using CreateDeviceAndSwapChainFn = long(__stdcall*)(void* adapter, unsigned int driver_type, void* software,
                                                    unsigned int flags, const void* feature_levels,
                                                    unsigned int feature_level_count,
                                                    unsigned int sdk_version, const void* swapchain_desc,
                                                    void** swapchain, void** device, void* feature_level,
                                                    void** immediate_context);

// The real System32\d3d11.dll (SysWOW64 for this 32-bit process).
class SystemD3d11 {
public:
    // Load the real d3d11.dll and resolve D3D11CreateDeviceAndSwapChain, the
    // one function AI.exe imports, which we intercept, plus the exports we
    // pass through verbatim for other modules, into `forward` by `names`.
    //
    // LoadLibrary under the loader lock is a deadlock hazard, so this runs
    // on the first call of any export, not in DllMain (01 §1.3).
    //
    // Returns true if LoadLibraryEx and GetProcAddress succeeded and the
    // returned handle is not our own DLL (see the implementation).
    bool load(std::span<void*> forward, std::span<const char* const> names,
              core::diag::ports::Log& log) noexcept;

    // Free the real d3d11.dll. Called from DLL_PROCESS_DETACH on FreeLibrary
    // (not at process exit).
    void shutdown() noexcept;

    // The real implementation. Null until load() succeeds.
    CreateDeviceAndSwapChainFn create_device_and_swapchain() const noexcept {
        return create_device_and_swapchain_;
    }

private:
    HINSTANCE__* self_                                      = nullptr; // this DLL's HMODULE
    HINSTANCE__* real_module_                               = nullptr; // owned: freed by shutdown()
    CreateDeviceAndSwapChainFn create_device_and_swapchain_ = nullptr;
};

} // namespace omvr::platform::d3d11
