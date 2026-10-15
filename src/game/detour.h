#pragma once

#include <cstdint>
#include <span>
#include <type_traits>

namespace omvr::game {

// Entry detours for game functions. The original only redirects call sites
// and vtable slots; features that must see every call of a function (event
// taps, animation commands) need its entry. MinHook relocates the
// overwritten prologue into a trampoline.
//
// `expected` is the function's first bytes as we know them: on a mismatch
// (another game build, another mod's hook) nothing is patched. `*original`
// receives the trampoline, through which `replacement` calls the game's
// function; it is set before the detour goes live. Returns false when
// nothing was patched.
bool detour(std::uint8_t* target, const void* replacement, std::span<const std::uint8_t> expected,
            void** original, const char* name) noexcept;

template <class Fn, class Replacement>
bool detour(std::uint8_t* target, Replacement replacement, std::span<const std::uint8_t> expected, Fn& original,
            const char* name) noexcept {
    static_assert(std::is_convertible_v<Replacement, Fn>, "the replacement must have the original's signature");
    return detour(target, reinterpret_cast<const void*>(replacement), expected,
                  reinterpret_cast<void**>(&original), name);
}

} // namespace omvr::game
