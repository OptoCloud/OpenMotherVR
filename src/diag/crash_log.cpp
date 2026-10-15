#include "crash_log.h"

#include "log.h"

#include <windows.h>

#include <atomic>
#include <cstdint>
#include <format>
#include <string_view>
#include <utility>

namespace mothervr::diag {

namespace {

LPTOP_LEVEL_EXCEPTION_FILTER g_previous = nullptr;
std::atomic<bool>            g_logged{false};

// A filter the game (or its C runtime) installed over ours after attach.
// Ours is put back on top and this one runs after the record, so neither
// the record nor the game's own handling is lost.
std::atomic<LPTOP_LEVEL_EXCEPTION_FILTER> g_game_filter{nullptr};
// The thread currently inside g_game_filter: if that filter chains to the
// one it replaced (ours), the nested call goes on to g_previous instead of
// looping back into it.
std::atomic<DWORD> g_chaining_thread{0};

// The record is rendered in static storage, not on the faulting thread's
// stack: after a stack overflow only the guard region is left, too little
// for log_line's buffer plus two MAX_PATH names. g_logged makes the record
// a one-off, so one set of buffers is enough.
char    g_message[1024];
char    g_line[1280];
wchar_t g_path[MAX_PATH];
char    g_name[MAX_PATH * 3]; // UTF-8 of a MAX_PATH wide name

template <class... Args>
void record(std::format_string<Args...> fmt, Args&&... args) noexcept {
    auto r = std::format_to_n(g_message, sizeof(g_message) - 1, fmt, std::forward<Args>(args)...);
    log_line_in(std::string_view{g_message, static_cast<std::size_t>(r.out - g_message)}, g_line, sizeof(g_line));
}

// Base and file name (UTF-8, into `name`) of the module holding `address`,
// without allocating: the process may be in any state here. Wide APIs, so a
// module path outside the ANSI code page is named correctly. `path` is
// scratch for the full path.
template <std::size_t N>
std::string_view module_of(const void* address, std::uintptr_t& base, wchar_t (&path)[MAX_PATH],
                           char (&name)[N]) noexcept {
    static_assert(N >= 2);
    base = 0;
    name[0] = '?';
    name[1] = '\0';
    HMODULE m = nullptr;
    if (!::GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                              static_cast<LPCWSTR>(address), &m))
        return {name, 1};
    base = reinterpret_cast<std::uintptr_t>(m);
    const DWORD n = ::GetModuleFileNameW(m, path, MAX_PATH);
    if (n == 0 || n >= MAX_PATH) return {name, 1};
    const wchar_t* file = path;
    for (DWORD i = 0; i < n; ++i)
        if (path[i] == L'\\' || path[i] == L'/') file = path + i + 1;
    const int len = ::WideCharToMultiByte(CP_UTF8, 0, file, static_cast<int>(path + n - file), name,
                                          static_cast<int>(N - 1), nullptr, nullptr);
    if (len <= 0) return {name, 1};
    name[len] = '\0';
    return {name, static_cast<std::size_t>(len)};
}

void write_record(const EXCEPTION_POINTERS& info) noexcept {
    const EXCEPTION_RECORD& r = *info.ExceptionRecord;
    const CONTEXT& c = *info.ContextRecord;
    std::uintptr_t base = 0;
    const std::string_view name = module_of(r.ExceptionAddress, base, g_path, g_name);
    const auto at = reinterpret_cast<std::uintptr_t>(r.ExceptionAddress);
    record("crash: exception 0x{:08X} at 0x{:08X} ({}+0x{:X}), thread {}", static_cast<unsigned>(r.ExceptionCode),
           at, name, base ? at - base : at, ::GetCurrentThreadId());
    if (r.ExceptionCode == EXCEPTION_ACCESS_VIOLATION && r.NumberParameters >= 2) {
        record("crash: {} of 0x{:08X}", r.ExceptionInformation[0] == 0 ? "read" : r.ExceptionInformation[0] == 1 ? "write" : "execute",
               static_cast<std::uintptr_t>(r.ExceptionInformation[1]));
    }
    // EIP too: it differs from the exception address for a fault raised
    // inside a called system routine, a breakpoint or a single step.
    record("crash: eax={:08X} ebx={:08X} ecx={:08X} edx={:08X} esi={:08X} edi={:08X} ebp={:08X} esp={:08X} eip={:08X}",
           c.Eax, c.Ebx, c.Ecx, c.Edx, c.Esi, c.Edi, c.Ebp, c.Esp, c.Eip);
    // The return addresses on top of the stack name the callers.
    // Read only within this thread's committed stack.
    const auto* stack = reinterpret_cast<const std::uintptr_t*>(c.Esp);
    const auto* tib   = reinterpret_cast<const NT_TIB*>(::NtCurrentTeb());
    const auto* top   = static_cast<const std::uintptr_t*>(tib->StackBase);
    constexpr int kStackWords = 16;
    for (int i = 0; i < kStackWords && stack + i < top && stack >= tib->StackLimit; ++i) {
        const std::uintptr_t value = stack[i];
        std::uintptr_t mod = 0;
        const std::string_view mod_name = module_of(reinterpret_cast<const void*>(value), mod, g_path, g_name);
        if (mod) record("crash: [esp+0x{:02X}] {}+0x{:X}", i * 4, mod_name, value - mod);
    }
}

LONG chain(EXCEPTION_POINTERS* info) noexcept {
    const DWORD self = ::GetCurrentThreadId();
    if (const auto game = g_game_filter.load(); game && g_chaining_thread.load() != self) {
        g_chaining_thread.store(self);
        const LONG result = game(info);
        g_chaining_thread.store(0);
        return result;
    }
    return g_previous ? g_previous(info) : EXCEPTION_CONTINUE_SEARCH;
}

LONG WINAPI on_crash(EXCEPTION_POINTERS* info) noexcept {
    // Only the first: a crash while logging must not recurse.
    if (info && info->ExceptionRecord && info->ContextRecord && !g_logged.exchange(true)) {
        // A fault while writing the record ends the record, not the
        // chain: the next filter still sees the original exception.
        __try {
            write_record(*info);
        } __except (EXCEPTION_EXECUTE_HANDLER) {
        }
    }
    return chain(info);
}

} // namespace

void install_crash_log() noexcept {
    g_previous = ::SetUnhandledExceptionFilter(&on_crash);
}

void reassert_crash_log() noexcept {
    const LPTOP_LEVEL_EXCEPTION_FILTER current = ::SetUnhandledExceptionFilter(&on_crash);
    if (current == &on_crash || (current && current == g_game_filter.load())) return;
    if (current) g_game_filter.store(current);
    // Logged once per change: a filter replaced again later is reported
    // again. Local buffers: the static ones belong to a crash on another
    // thread.
    std::uintptr_t base = 0;
    wchar_t path[MAX_PATH];
    char name[MAX_PATH * 3];
    const std::string_view owner = current ? module_of(reinterpret_cast<const void*>(current), base, path, name)
                                            : std::string_view{"none"};
    logf("crash: the unhandled-exception filter was replaced (by 0x{:08X}, {}+0x{:X}); ours is back on top{}",
         reinterpret_cast<std::uintptr_t>(current), owner,
         base ? reinterpret_cast<std::uintptr_t>(current) - base : 0,
         current ? " and runs it after the record" : "");
}

} // namespace mothervr::diag
