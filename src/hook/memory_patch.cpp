// Memory-patch primitives: protection-cycled WriteProcessMemory.
//
// From: FUN_1001d4d0 @ 0x1001d4d0 plus the inlined Protect/Write/Restore
// triples scattered through FUN_1001d6f0.

#include "memory_patch.h"
#include "diag/log.h"

#include <windows.h>

namespace mothervr::hook {

bool write_bytes(void* dst, const void* src, std::size_t n) noexcept {
    const HANDLE self = ::GetCurrentProcess();
    DWORD old = 0;
    if (!::VirtualProtectEx(self, dst, n, PAGE_EXECUTE_READWRITE, &old)) {
        diag::logf("[hook] cannot unprotect {} bytes at {}: error {}", n, dst, ::GetLastError());
        return false;
    }
    // WriteProcessMemory flushes the instruction cache for the pages it writes.
    SIZE_T written = 0;
    const bool ok = ::WriteProcessMemory(self, dst, src, n, &written) && written == n;
    if (!ok) {
        diag::logf("[hook] write of {} bytes at {} failed: error {}", n, dst, ::GetLastError());
    }
    // Only reached when the protection was changed, so `old` is real.
    if (DWORD unused = 0; !::VirtualProtectEx(self, dst, n, old, &unused)) {
        diag::logf("[hook] cannot restore protection at {}: error {}", dst, ::GetLastError());
    }
    return ok;
}

bool write_dword(void* dst, std::uint32_t value) noexcept {
    return write_bytes(dst, &value, sizeof(value));
}

} // namespace mothervr::hook
