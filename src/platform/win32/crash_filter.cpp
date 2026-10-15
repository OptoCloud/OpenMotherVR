// OpenMotherVR-Provenance: clean
#include "platform/win32/crash_filter.h"

#include <windows.h>

#include <cstdint>
#include <span>
#include <string_view>

namespace omvr::platform::win32 {

namespace {

static_assert(kMaxPath == MAX_PATH);

// The record's lines (spec 11: the crash record). Constants so a tester can
// search the source for a line found in a log.
constexpr std::string_view kNoModule = "?";
constexpr std::string_view kNoFilter = "none";

// Base and file name (UTF-8, into `name`) of the module holding `address`,
// without allocating: the process may be in any state here. Wide APIs, so a
// module path outside the ANSI code page is named correctly. `path` is
// scratch for the full path.
template <std::size_t N>
std::string_view module_of(const void* address, std::uintptr_t& base, wchar_t (&path)[MAX_PATH],
                           char (&name)[N]) noexcept {
    static_assert(N > kNoModule.size());
    base = 0;
    kNoModule.copy(name, kNoModule.size());
    name[kNoModule.size()] = '\0';
    const std::string_view unknown{name, kNoModule.size()};
    HMODULE m = nullptr;
    if (!::GetModuleHandleExW(
            GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
            static_cast<LPCWSTR>(address), &m))
        return unknown;
    base          = reinterpret_cast<std::uintptr_t>(m);
    const DWORD n = ::GetModuleFileNameW(m, path, MAX_PATH);
    if (n == 0 || n >= MAX_PATH) return unknown;
    const wchar_t* file = path;
    for (DWORD i = 0; i < n; ++i)
        if (path[i] == L'\\' || path[i] == L'/') file = path + i + 1;
    const int len = ::WideCharToMultiByte(CP_UTF8, 0, file, static_cast<int>(path + n - file), name,
                                          static_cast<int>(N - 1), nullptr, nullptr);
    if (len <= 0) return unknown;
    name[len] = '\0';
    return {name, static_cast<std::size_t>(len)};
}

// The word of an access violation's first parameter, as the record names it.
std::string_view access_kind(std::uintptr_t kind) noexcept {
    if (kind == kAccessViolationRead) return "read";
    if (kind == kAccessViolationWrite) return "write";
    return "execute";
}

} // namespace

template <class... Args>
void CrashRecorder::record(std::format_string<Args...> fmt, Args&&... args) noexcept {
    auto r = std::format_to_n(message_, sizeof(message_) - 1, fmt, std::forward<Args>(args)...);
    log_.line_in(std::string_view{message_, static_cast<std::size_t>(r.out - message_)}, line_);
}

void CrashRecorder::write_record(const EXCEPTION_POINTERS& info) noexcept {
    const EXCEPTION_RECORD& r   = *info.ExceptionRecord;
    const CONTEXT& c            = *info.ContextRecord;
    std::uintptr_t base         = 0;
    const std::string_view name = module_of(r.ExceptionAddress, base, path_, name_);
    const auto at               = reinterpret_cast<std::uintptr_t>(r.ExceptionAddress);
    record("crash: exception 0x{:08X} at 0x{:08X} ({}+0x{:X}), thread {}",
           static_cast<unsigned>(r.ExceptionCode), at, name, base ? at - base : at, ::GetCurrentThreadId());
    // An access violation's parameters: the kind of access and the address.
    if (r.ExceptionCode == EXCEPTION_ACCESS_VIOLATION && r.NumberParameters >= 2) {
        record("crash: {} of 0x{:08X}", access_kind(r.ExceptionInformation[0]),
               static_cast<std::uintptr_t>(r.ExceptionInformation[1]));
    }
    // EIP too: it differs from the exception address for a fault raised
    // inside a called system routine, a breakpoint or a single step.
    record(
        "crash: eax={:08X} ebx={:08X} ecx={:08X} edx={:08X} esi={:08X} edi={:08X} ebp={:08X} esp={:08X} "
        "eip={:08X}",
        c.Eax, c.Ebx, c.Ecx, c.Edx, c.Esi, c.Edi, c.Ebp, c.Esp, c.Eip);
    write_stack(info);
}

// The return addresses on top of the stack name the callers. Read only
// within this thread's committed stack.
void CrashRecorder::write_stack(const EXCEPTION_POINTERS& info) noexcept {
    const auto* stack = reinterpret_cast<const std::uintptr_t*>(info.ContextRecord->Esp);
    const auto* tib   = reinterpret_cast<const NT_TIB*>(::NtCurrentTeb());
    const auto* top   = static_cast<const std::uintptr_t*>(tib->StackBase);
    for (int i = 0; i < kCrashStackWords && stack + i < top && stack >= tib->StackLimit; ++i) {
        const std::uintptr_t value      = stack[i];
        std::uintptr_t mod              = 0;
        const std::string_view mod_name = module_of(reinterpret_cast<const void*>(value), mod, path_, name_);
        if (mod) {
            record("crash: [esp+0x{:02X}] {}+0x{:X}", i * static_cast<int>(sizeof(std::uintptr_t)), mod_name,
                   value - mod);
        }
    }
}

long CrashRecorder::chain(EXCEPTION_POINTERS* info) noexcept {
    const DWORD self = ::GetCurrentThreadId();
    if (const auto game = game_filter_.load(); game && chaining_thread_.load() != self) {
        chaining_thread_.store(self);
        const LONG result = game(info);
        chaining_thread_.store(0);
        return result;
    }
    return previous_ ? previous_(info) : EXCEPTION_CONTINUE_SEARCH;
}

long CrashRecorder::on_crash(EXCEPTION_POINTERS* info) noexcept {
    // Only the first: a crash while logging must not recurse.
    if (info && info->ExceptionRecord && info->ContextRecord && !logged_.exchange(true)) {
        // A fault while writing the record ends the record, not the
        // chain: the next filter still sees the original exception.
        __try {
            write_record(*info);
        } __except (EXCEPTION_EXECUTE_HANDLER) {}
    }
    return chain(info);
}

void CrashRecorder::install(Filter ours) noexcept { previous_ = ::SetUnhandledExceptionFilter(ours); }

void CrashRecorder::reassert(Filter ours) noexcept {
    const Filter current = ::SetUnhandledExceptionFilter(ours);
    if (current == ours || (current && current == game_filter_.load())) return;
    if (current) game_filter_.store(current);
    // Logged once per change: a filter replaced again later is reported
    // again. Local buffers: the member ones belong to a crash on another
    // thread.
    std::uintptr_t base = 0;
    wchar_t path[MAX_PATH];
    char name[MAX_PATH * kUtf8BytesPerUtf16Unit];
    const auto address = reinterpret_cast<std::uintptr_t>(current);
    const std::string_view owner =
        current ? module_of(reinterpret_cast<const void*>(current), base, path, name) : kNoFilter;
    core::diag::ports::logf(
        log_,
        "crash: the unhandled-exception filter was replaced (by 0x{:08X}, {}+0x{:X}); ours is back on top{}",
        address, owner, base ? address - base : 0, current ? " and runs it after the record" : "");
}

} // namespace omvr::platform::win32
