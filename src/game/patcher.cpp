// Memory-patch primitives: protection-cycled WriteProcessMemory.
//
// From: FUN_1001d4d0 @ 0x1001d4d0 plus the inlined Protect/Write/Restore
// triples scattered through FUN_1001d6f0.

#include "game/patcher.h"

#include "platform/win32/patching/memory.h"

namespace omvr::game {

bool write_bytes(void* dst, const void* src, std::size_t n) noexcept {
    return platform::win32::write_memory(dst, src, n);
}

bool write_dword(void* dst, std::uint32_t value) noexcept {
    return write_bytes(dst, &value, sizeof(value));
}

} // namespace omvr::game
