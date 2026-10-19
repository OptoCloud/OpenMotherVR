// Memory-patch primitives: protection-cycled WriteProcessMemory.
//
// From: FUN_1001d4d0 @ 0x1001d4d0 plus the inlined Protect/Write/Restore
// triples scattered through FUN_1001d6f0.

#include "game/patcher.h"

#include "game/x86.h"
#include "platform/win32/patching/memory.h"

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
    return write_bytes(dst, x86::le32(value));
}

util::Result<void> Patcher::redirect_call(std::uint8_t* site, const void* fn,
                                          std::string_view what) noexcept {
    if (*site != x86::kCallRel32) {
        core::diag::ports::logf(log_, kNoCallLog, what, static_cast<void*>(site));
        return std::unexpected(util::Error{.code = 0, .context = "no call at the site"});
    }
    const std::uint32_t rel = rel32(site + x86::kCallRel32Size, fn);
    if (auto written = write_u32(site + x86::kCallOperandOffset, rel); !written) {
        core::diag::ports::logf(log_, kNotRedirected, what, static_cast<void*>(site));
        return written;
    }
    return {};
}

bool redirect_all(Patcher& p, std::span<const CallRedirect> calls) noexcept {
    for (const CallRedirect& c : calls) {
        if (!p.redirect_call(c.site, c.fn, c.what)) return false;
    }
    return true;
}

} // namespace omvr::game
