#include "platform/win32/patching/memory.h"

#include "core/diag/logger.h" // R: platform (A2): log through an injected Log&

#include <windows.h>

#include <atomic>

namespace omvr::platform::win32 {

namespace diag = core::diag;

void pin_this_module() noexcept {
    static std::atomic<bool> s_pinned{false};
    if (s_pinned.exchange(true)) return;
    HMODULE self = nullptr;
    if (!::GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_PIN,
                              reinterpret_cast<LPCWSTR>(&pin_this_module), &self)) {
        diag::logf("[hook] cannot pin the mod's module: error {}", ::GetLastError());
    }
}

bool write_memory(void* dst, const void* src, std::size_t n) noexcept {
    // Before the first write that could leave game code pointing into us.
    pin_this_module();
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

void* replace_pointer(void** slot, void* replacement, const char* method) noexcept {
    DWORD old_protect = 0;
    if (!::VirtualProtect(slot, sizeof(void*), PAGE_READWRITE, &old_protect)) {
        diag::logf("proxy: interception of {} failed: VirtualProtect error {}", method,
                   ::GetLastError());
        return nullptr;
    }
    // From here the slot points into this DLL for the rest of the process.
    pin_this_module();
    void* prev = *slot;
    *slot = replacement;
    DWORD ignore = 0;
    ::VirtualProtect(slot, sizeof(void*), old_protect, &ignore);
    return prev;
}

} // namespace omvr::platform::win32
