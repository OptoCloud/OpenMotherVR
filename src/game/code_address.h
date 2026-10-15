#pragma once

#include "game/x86.h"

#include <cstdint>
#include <cstring>
#include <type_traits>

// Addresses in the running process as AI.exe sees them: 32-bit values. The
// casts between pointers and those values are here and nowhere else in
// game/ (STYLEGUIDE 4: one reinterpret_cast helper per kind).
namespace omvr::game {

static_assert(sizeof(void*) == sizeof(std::uint32_t), "the game and the mod are 32-bit");

// Bytes in one vtable slot, a code pointer or a disp32.
inline constexpr std::uint32_t kPointerSize = sizeof(std::uint32_t);

// The 32-bit address of `p`.
inline std::uint32_t address_of(const volatile void* p) noexcept {
    return static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(p));
}

// The 32-bit address of a function of the mod, to write into the game.
template <class Fn>
    requires std::is_function_v<Fn>
std::uint32_t code_address(Fn* fn) noexcept {
    return static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(fn));
}

// A function's address as an untyped pointer, for the tables of
// replacements the installer hands to the patch sites.
template <class Fn>
    requires std::is_function_v<Fn>
const void* code_pointer(Fn* fn) noexcept {
    return reinterpret_cast<const void*>(fn);
}

// A game address that converts to whichever pointer type its receiver
// declares: a function of AI.exe or an object in its image. The receiver's
// type says how the game's code is called, so it is written once, there.
class CodeAddress {
public:
    explicit CodeAddress(const volatile void* p) noexcept : va_{address_of(p)} {}
    explicit CodeAddress(std::uint32_t va) noexcept : va_{va} {}

    template <class T>
        requires std::is_pointer_v<T>
    operator T() const noexcept { // NOLINT(google-explicit-constructor): the point of the type
        return reinterpret_cast<T>(static_cast<std::uintptr_t>(va_));
    }

private:
    std::uint32_t va_;
};

// The value of the 4 bytes at `p` (unaligned).
inline std::uint32_t read_u32(const std::uint8_t* p) noexcept {
    std::uint32_t v;
    std::memcpy(&v, p, sizeof(v));
    return v;
}

// The code or data address stored in the slot at `slot` (a vtable entry, a
// global pointer).
inline CodeAddress pointer_at(const std::uint8_t* slot) noexcept { return CodeAddress{read_u32(slot)}; }

// Byte offset of vtable slot `index` from the vtable's start.
constexpr std::uint32_t slot_offset(std::uint32_t index) noexcept { return index * kPointerSize; }

// Target of the `call rel32` at `site`.
inline std::uint32_t call_target(const std::uint8_t* site) noexcept {
    return address_of(site + x86::kCallRel32Size) + read_u32(site + x86::kCallOperandOffset);
}

// True if `site` holds `call rel32` to `target`: the code is what a fix that
// redirects the call was written for (12 §4.1).
inline bool calls(const std::uint8_t* site, const std::uint8_t* target) noexcept {
    return *site == x86::kCallRel32 && call_target(site) == address_of(target);
}

} // namespace omvr::game
