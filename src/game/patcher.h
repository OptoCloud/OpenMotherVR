#pragma once

#include "core/diag/ports/log.h"
#include "game/code_address.h"
#include "util/base/error.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string_view>

// Writes to AI.exe's code and read-only data: code patches, call redirects
// and vtable slots. Every write to the game goes through here, and so
// through platform::win32::write_memory (STYLEGUIDE 6, 12 §4).
namespace omvr::game {

class Patcher {
public:
    explicit Patcher(core::diag::ports::Log& log) noexcept : log_(log) {}

    // The log the writes report to; the patch sites log their outcome there.
    core::diag::ports::Log& log() const noexcept { return log_; }

    // Arbitrary-size byte patch. Fails, after logging the Windows error, if
    // the pages could not be made writable or the write failed.
    // From: the Protect/Write/Restore triples inlined through FUN_1001d6f0.
    util::Result<void> write_bytes(std::uint8_t* dst, std::span<const std::uint8_t> bytes) noexcept;

    // 4-byte write: function pointer, vtable slot, rel32 or disp32.
    // From: FUN_1001d4d0 @ 0x1001d4d0 (the explicit helper) and dozens of
    // inlined copies inside FUN_1001d6f0.
    util::Result<void> write_u32(std::uint8_t* dst, std::uint32_t value) noexcept;

    // A pointer (a replacement method, a string the game is pointed at)
    // into the slot or operand at `slot`.
    util::Result<void> write_pointer(std::uint8_t* slot, const volatile void* p) noexcept {
        return write_u32(slot, address_of(p));
    }

    template <std::size_t N>
    util::Result<void> write(std::uint8_t* site, const std::array<std::uint8_t, N>& bytes) noexcept {
        return write_bytes(site, bytes);
    }

    // Points the `call rel32` at `site` to `fn`, which must have the
    // callee's ABI. Fails, logged under `what`, when `site` holds no call.
    util::Result<void> redirect_call(std::uint8_t* site, const void* fn, std::string_view what) noexcept;

private:
    core::diag::ports::Log& log_;
};

// A `call rel32` to point at a replacement with the callee's ABI, and its
// name for the patcher's log.
struct CallRedirect {
    std::uint8_t* site;
    const void* fn;
    std::string_view what;
};

// Redirects `calls` in order and stops at the first that fails; false then.
bool redirect_all(Patcher& patcher, std::span<const CallRedirect> calls) noexcept;

} // namespace omvr::game
