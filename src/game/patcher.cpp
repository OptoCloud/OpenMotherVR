// Memory-patch primitives: protection-cycled WriteProcessMemory.
//
// From: FUN_1001d4d0 @ 0x1001d4d0 plus the inlined Protect/Write/Restore
// triples scattered through FUN_1001d6f0.

#include "game/patcher.h"

#include "game/x86.h"
#include "platform/win32/patching/memory.h"

#include <array>
#include <cstring>
#include <expected>

namespace omvr::game {

namespace {

constexpr std::string_view kNoCallLog     = "[hook] {}: no call at {}, not patched";
constexpr std::string_view kNotRedirected = "[hook] {}: call at {} not redirected";

} // namespace

util::Result<void> Patcher::write_bytes(std::uint8_t* dst, std::span<const std::uint8_t> bytes) noexcept {
    return platform::win32::write_memory(dst, bytes, log_);
}

util::Result<void> Patcher::write_u32(std::uint8_t* dst, std::uint32_t value) noexcept {
    std::array<std::uint8_t, sizeof(value)> bytes{};
    std::memcpy(bytes.data(), &value, sizeof(value));
    return write_bytes(dst, bytes);
}

util::Result<void> Patcher::redirect_call(std::uint8_t* site, const void* fn, std::string_view what) noexcept {
    if (*site != x86::kCallRel32) {
        core::diag::ports::logf(log_, kNoCallLog, what, static_cast<void*>(site));
        return std::unexpected(util::Error{.code = 0, .context = "no call at the site"});
    }
    // rel32 counts from the end of the call instruction.
    const std::uint32_t rel = address_of(fn) - address_of(site + x86::kCallRel32Size);
    if (auto written = write_u32(site + x86::kCallOperandOffset, rel); !written) {
        core::diag::ports::logf(log_, kNotRedirected, what, static_cast<void*>(site));
        return written;
    }
    return {};
}

} // namespace omvr::game
