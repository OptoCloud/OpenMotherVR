#include "game/clear_gate.h"

#include "core/diag/logger.h" // R: game (A2): log through an injected Log&
#include "game/patcher.h"
// R: game (A2): the executable stub through platform/win32/patching/memory

#include <windows.h>

#include <cstring>
#include <initializer_list>

namespace omvr::game {

namespace {

namespace diag = core::diag;
namespace hook {
using game::write_bytes;
} // namespace hook

// Read by the clear gate's machine code. R: stereo (A3): an atomic byte at a
// stable address.
volatile std::uint8_t g_skip_clears = 0;

} // namespace

bool install_clear_gate(std::uint8_t* clear) noexcept {
    // push ebp; mov ebp,esp; sub esp,0x10
    constexpr std::uint8_t kPrologue[6] = {0x55, 0x8B, 0xEC, 0x83, 0xEC, 0x10};
    if (std::memcmp(clear, kPrologue, sizeof(kPrologue)) != 0) {
        diag::log_line("[stereo] clear gate: unexpected code, reflections stay mono");
        return false;
    }
    auto* stub = static_cast<std::uint8_t*>(
        ::VirtualAlloc(nullptr, 64, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE));
    if (!stub) return false;
    std::uint8_t* p = stub;
    auto put = [&p](std::initializer_list<std::uint8_t> bytes) {
        for (auto b : bytes) *p++ = b;
    };
    auto put32 = [&p](std::uint32_t v) {
        std::memcpy(p, &v, 4);
        p += 4;
    };
    const auto addr = [](const volatile void* a) {
        return static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(a));
    };
    put({0x80, 0x3D}); put32(addr(&g_skip_clears)); put({0x00});  // cmp byte ptr [g_skip_clears], 0
    put({0x75, 0x0B});                                             // jne skip
    put({0x55, 0x8B, 0xEC, 0x83, 0xEC, 0x10});                     // the clear's prologue
    put({0xE9}); put32(addr(clear + 6) - addr(p + 4));             // jmp clear+6
    put({0xC3});                                                   // skip: ret
    DWORD old_protect = 0;
    ::VirtualProtect(stub, 64, PAGE_EXECUTE_READ, &old_protect);   // written once; no W+X
    ::FlushInstructionCache(::GetCurrentProcess(), stub, 64);

    std::uint8_t entry[6] = {0xE9, 0, 0, 0, 0, 0x90};              // jmp stub; nop
    const std::uint32_t rel = addr(stub) - addr(clear + 5);
    std::memcpy(entry + 1, &rel, 4);
    return hook::write_bytes(clear, entry, sizeof(entry));
}

void set_skip_clears(bool skip) noexcept { g_skip_clears = skip; }

} // namespace omvr::game
