// The d3d11.dll exports the proxy passes through verbatim (01 §1.3).

#include "loader/d3d11_forwarders.h"

#include "loader/context.h"

#include <windows.h>

#include <mutex>
#include <span>

// ===== Verbatim forwarders ===================================================
// One slot per pass-through export, filled by ensure_d3d11_loaded() on the
// first call of any export. The thunks live at file scope (not in a
// namespace) so MSVC inline asm can name the table and the .def can alias
// each export to its mvr_fwd_* symbol.

#define OMVR_D3D11_FORWARD(i, name) +1
constexpr unsigned kForwardCount = 0
#include "loader/d3d11_exports.inc"
    ;
#undef OMVR_D3D11_FORWARD

// The thunks index the table in 4-byte steps (`(i) * 4` below).
static_assert(sizeof(void*) == 4, "the forwarder thunks are x86 code");

// Read by the thunks' `jmp dword ptr [forward_slots + i*4]`, so a plain
// array at a fixed address. Written once, before forward_ready is set.
static void* forward_slots[kForwardCount] = {};

// Nonzero once ensure_d3d11_loaded() has run (whether or not it succeeded).
// The naked thunk compares it in assembly, so it stays a plain aligned long:
// an aligned 32-bit read is atomic on x86, and it is written once with
// InterlockedExchange (a full barrier) after the table is filled.
static volatile long forward_ready = 0;

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
        cmp forward_ready, 0
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
        __asm jmp dword ptr [forward_slots + (i) * 4]                  \
    }
#include "loader/d3d11_exports.inc"
#undef OMVR_D3D11_FORWARD

namespace omvr::loader {

bool ensure_d3d11_loaded() noexcept {
    static std::once_flag once;
    static bool           loaded = false;
    try {
        std::call_once(once, [] {
            auto& ctx = context();
            loaded = ctx.system_d3d11.load(std::span<void*>(forward_slots), std::span<const char* const>(kForwardNames),
                                           ctx.log);
            ::InterlockedExchange(&forward_ready, 1);
        });
    } catch (...) {
        // call_once only throws if the callable does, and it does not.
    }
    return loaded;
}

namespace entry {

platform::d3d11::CreateDeviceAndSwapChainFn real_create_device_and_swapchain() noexcept {
    return context().system_d3d11.create_device_and_swapchain();
}

void wait_for_dialogs() noexcept { context().dialogs.wait_posted(); }

bool ui_set_only() noexcept { return context().ui_set_only.load(std::memory_order_acquire); }

bool headset_attached() noexcept { return context().window.headset_attached(); }

bool mirror_size(std::uint32_t& width, std::uint32_t& height) noexcept {
    return context().window.mirror_size(width, height);
}

core::diag::ports::Log& log() noexcept { return context().log; }

} // namespace entry

} // namespace omvr::loader
