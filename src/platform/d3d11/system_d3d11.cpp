// The real d3d11.dll behind the proxy.
//
// The original MotherVR is a dxgi.dll proxy. Windows 11 will not load a
// local dxgi.dll, but it does load a local d3d11.dll (see ../../README.md),
// so this rebuild proxies d3d11 instead.

#include "platform/d3d11/system_d3d11.h"

#include "core/diag/logger.h" // R: proxy (A2): log through an injected Log&
#include "platform/d3d11/constants.h"
#include "platform/win32/proc.h"

#include <windows.h>

namespace omvr::platform::d3d11 {

namespace {

namespace diag = core::diag;
namespace util = win32;

} // namespace

bool SystemD3d11::load(std::span<void*> forward, std::span<const char* const> names) noexcept {
    ::GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                         reinterpret_cast<LPCWSTR>(&kSystemD3d11), &g_self);

    // Build absolute path to the real DLL. Passing just "d3d11.dll" to
    // LoadLibrary matches by basename in the loaded-modules list and would
    // return OUR handle, causing infinite recursion on the first forward.
    wchar_t sysdir[MAX_PATH];
    UINT sn = ::GetSystemDirectoryW(sysdir, MAX_PATH);
    if (sn == 0 || sn > MAX_PATH - 12) {
        diag::log_line("proxy: GetSystemDirectory failed");
        return false;
    }
    const wchar_t (&suffix)[11] = kSystemD3d11;
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
    for (unsigned i = 0; i < forward.size(); ++i) {
        forward[i] = util::proc<void*>(g_real_module, names[i]);
        if (!forward[i]) {
            diag::logf("proxy: real d3d11 lacks export {}", names[i]);
            ++missing;
        }
    }
    diag::logf("proxy: {} pass-through exports resolved, {} missing",
               forward.size() - missing, missing);
    return true;
}

void SystemD3d11::shutdown() noexcept {
    if (g_real_module) {
        ::FreeLibrary(g_real_module);
        g_real_module = nullptr;
    }
}

} // namespace omvr::platform::d3d11
