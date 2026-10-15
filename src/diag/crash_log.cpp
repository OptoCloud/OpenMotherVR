#include "crash_log.h"

#include "log.h"

#include <windows.h>

#include <atomic>
#include <cstdint>

namespace mothervr::diag {

namespace {

LPTOP_LEVEL_EXCEPTION_FILTER g_previous = nullptr;
std::atomic<bool>            g_logged{false};

// Base and file name of the module holding `address`, without allocating:
// the process may be in any state here.
void module_of(const void* address, std::uintptr_t& base, char (&name)[MAX_PATH]) noexcept {
    base = 0;
    name[0] = '?';
    name[1] = '\0';
    HMODULE m = nullptr;
    if (!::GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                              static_cast<LPCSTR>(address), &m))
        return;
    base = reinterpret_cast<std::uintptr_t>(m);
    char path[MAX_PATH] = {};
    const DWORD n = ::GetModuleFileNameA(m, path, MAX_PATH);
    const char* file = path;
    for (DWORD i = 0; i < n; ++i)
        if (path[i] == '\\' || path[i] == '/') file = path + i + 1;
    lstrcpynA(name, file, MAX_PATH);
}

LONG WINAPI on_crash(EXCEPTION_POINTERS* info) noexcept {
    // Only the first: a crash while logging must not recurse.
    if (info && !g_logged.exchange(true)) {
        const EXCEPTION_RECORD& r = *info->ExceptionRecord;
        const CONTEXT& c = *info->ContextRecord;
        std::uintptr_t base = 0;
        char name[MAX_PATH];
        module_of(r.ExceptionAddress, base, name);
        const auto at = reinterpret_cast<std::uintptr_t>(r.ExceptionAddress);
        logf("crash: exception 0x{:08X} at 0x{:08X} ({}+0x{:X}), thread {}", static_cast<unsigned>(r.ExceptionCode),
             at, name, base ? at - base : at, ::GetCurrentThreadId());
        if (r.ExceptionCode == EXCEPTION_ACCESS_VIOLATION && r.NumberParameters >= 2) {
            logf("crash: {} of 0x{:08X}", r.ExceptionInformation[0] == 0 ? "read" : r.ExceptionInformation[0] == 1 ? "write" : "execute",
                 static_cast<std::uintptr_t>(r.ExceptionInformation[1]));
        }
        logf("crash: eax={:08X} ebx={:08X} ecx={:08X} edx={:08X} esi={:08X} edi={:08X} ebp={:08X} esp={:08X}",
             c.Eax, c.Ebx, c.Ecx, c.Edx, c.Esi, c.Edi, c.Ebp, c.Esp);
        // The return addresses on top of the stack name the callers.
        // Read only within this thread's committed stack.
        const auto* stack = reinterpret_cast<const std::uintptr_t*>(c.Esp);
        const auto* tib   = reinterpret_cast<const NT_TIB*>(::NtCurrentTeb());
        const auto* top   = static_cast<const std::uintptr_t*>(tib->StackBase);
        constexpr int kStackWords = 16;
        for (int i = 0; i < kStackWords && stack + i < top && stack >= tib->StackLimit; ++i) {
            const std::uintptr_t value = stack[i];
            std::uintptr_t mod = 0;
            char mod_name[MAX_PATH];
            module_of(reinterpret_cast<const void*>(value), mod, mod_name);
            if (mod) logf("crash: [esp+0x{:02X}] {}+0x{:X}", i * 4, mod_name, value - mod);
        }
    }
    return g_previous ? g_previous(info) : EXCEPTION_CONTINUE_SEARCH;
}

} // namespace

void install_crash_log() noexcept {
    g_previous = ::SetUnhandledExceptionFilter(&on_crash);
}

} // namespace mothervr::diag
