#pragma once

#include "core/diag/logger.h" // R: game (A2): log through an injected Log&
#include "game/constants.h"

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <initializer_list>
#include <iterator>

// Writes to AI.exe's code and read-only data: code patches, call redirects
// and vtable slots. Every write goes through here (and so through
// platform::win32::write_memory).
//
// The original used the pseudo-handle 0xFFFFFFFF; ::GetCurrentProcess() is
// the same value.
//
// Each returns false, after logging GetLastError(), if the pages could not
// be made writable or the write failed.
namespace omvr::game {

// Arbitrary-size byte patch.
// From: the Protect/Write/Restore triples inlined through FUN_1001d6f0.
bool write_bytes(void* dst, const void* src, std::size_t n) noexcept;

// 4-byte write: function pointer, vtable slot, rel32 or disp32.
// From: FUN_1001d4d0 @ 0x1001d4d0 (the explicit helper) and dozens of inlined
// copies inside FUN_1001d6f0.
bool write_dword(void* dst, std::uint32_t value) noexcept;

// The 32-bit address of `p`.
inline std::uint32_t address_of(const void* p) noexcept {
    return static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(p));
}

inline std::uint32_t read_u32(const std::uint8_t* p) noexcept {
    std::uint32_t v;
    std::memcpy(&v, p, sizeof(v));
    return v;
}

template <class Fn>
bool write_fn(std::uint8_t* slot, Fn* fn) noexcept {
    return write_dword(slot, address_of(reinterpret_cast<const void*>(fn)));
}

inline bool patch(std::uint8_t* site, std::initializer_list<std::uint8_t> bytes) noexcept {
    return write_bytes(site, std::data(bytes), bytes.size());
}

// Target of the `call rel32` at `site`.
inline std::uint32_t call_target(const std::uint8_t* site) noexcept {
    return address_of(site + 5) + read_u32(site + 1);
}

// True if `site` holds `call rel32` to `target`: the code is what a fix
// that redirects the call was written for.
inline bool calls(const std::uint8_t* site, const std::uint8_t* target) noexcept {
    return *site == kCall && call_target(site) == address_of(target);
}

// Point the `call rel32` at `site` to `fn`. The replacement must have the
// original callee's ABI.
template <class Fn>
bool redirect_call(std::uint8_t* site, Fn* fn, const char* what) noexcept {
    if (*site != kCall) {
        core::diag::logf("[hook] {}: no call at {}, not patched", what, static_cast<void*>(site));
        return false;
    }
    const auto target = address_of(reinterpret_cast<const void*>(fn));
    if (!write_dword(site + 1, target - address_of(site + 5))) {
        core::diag::logf("[hook] {}: call at {} not redirected", what, static_cast<void*>(site));
        return false;
    }
    return true;
}

} // namespace omvr::game
