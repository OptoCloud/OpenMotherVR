#pragma once

#include "core/diag/ports/log.h"

#include <cstdint>
#include <span>
#include <type_traits>

namespace omvr::game {

// Entry detours for game functions, for features that must see every call
// of a function (event taps), where redirecting call sites or vtable slots
// would miss some. MinHook relocates the overwritten prologue into a
// trampoline.
//
// `expected` is the function's first bytes as we know them: on a mismatch
// (another game build, another mod's hook) nothing is patched (12 §4.1).
// `*trampoline` receives the trampoline, through which `replacement` calls
// the game's function; it is set before the detour goes live. Returns false
// when nothing was patched.
bool detour(std::uint8_t* target, const void* replacement, std::span<const std::uint8_t> expected,
            void** trampoline, const char* name, core::diag::ports::Log& log) noexcept;

template <class Fn, class Replacement>
bool detour(std::uint8_t* target, Replacement replacement, std::span<const std::uint8_t> expected, Fn& trampoline,
            const char* name, core::diag::ports::Log& log) noexcept {
    static_assert(std::is_convertible_v<Replacement, Fn>, "the replacement must have the game function's signature");
    static_assert(std::is_pointer_v<Fn> && std::is_function_v<std::remove_pointer_t<Fn>>);
    return detour(target, reinterpret_cast<const void*>(replacement), expected, reinterpret_cast<void**>(&trampoline),
                  name, log);
}

} // namespace omvr::game
