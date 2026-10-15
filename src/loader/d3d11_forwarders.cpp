// The d3d11.dll exports the proxy passes through verbatim.

#include "loader/d3d11_forwarders.h"

#include "loader/context.h"

#include <windows.h>

#include <mutex>
#include <span>

// ===== Verbatim forwarders ===================================================
// One slot per pass-through export, filled by ensure_initialized() on the
// first call of any export. The thunks live at file scope (not in a
// namespace) so MSVC inline asm can name the table and the .def can alias
// each export to its mvr_fwd_* symbol.


#define OMVR_D3D11_FORWARD(i, name) +1
constexpr unsigned kForwardCount = 0
#include "loader/d3d11_exports.inc"
    ;
#undef OMVR_D3D11_FORWARD

static void* g_forward[kForwardCount] = {};

// Nonzero once ensure_initialized() has run (whether or not it succeeded).
static volatile long g_forward_ready = 0;


#define OMVR_D3D11_FORWARD(i, name) #name,
static const char* const kForwardNames[kForwardCount] = {
#include "loader/d3d11_exports.inc"
};
#undef OMVR_D3D11_FORWARD

extern "C" void __cdecl mvr_forward_init() {
    (void) omvr::loader::ensure_d3d11_loaded();
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
#define OMVR_D3D11_FORWARD(i, name)                                    \
    extern "C" __declspec(naked) void mvr_fwd_##name() {               \
        __asm call mvr_forward_ensure                                  \
        __asm jmp dword ptr [g_forward + (i) * 4]                      \
    }
#include "loader/d3d11_exports.inc"
#undef OMVR_D3D11_FORWARD

namespace omvr::loader {

bool ensure_d3d11_loaded() noexcept {
    static std::once_flag s_once;
    static bool s_ok = false;
    try {
        std::call_once(s_once, [] {
            s_ok = context().system_d3d11.load(std::span<void*>(g_forward), std::span<const char* const>(kForwardNames));
            ::InterlockedExchange(&g_forward_ready, 1);
        });
    } catch (...) {
        // call_once only throws if the callable does, and it does not.
    }
    return s_ok;
}

namespace entry {

platform::d3d11::PFN_D3D11CreateDeviceAndSwapChain real_create_device_and_swapchain() noexcept {
    return context().system_d3d11.create_device_and_swapchain();
}

void wait_for_dialogs() noexcept { context().dialogs.wait_posted(); }

bool ui_set_only() noexcept { return context().ui_set_only.load(std::memory_order_acquire); }

bool headset_attached() noexcept { return context().window.headset_attached(); }

bool mirror_size(std::uint32_t& width, std::uint32_t& height) noexcept {
    return context().window.mirror_size(width, height);
}

} // namespace entry

} // namespace omvr::loader
