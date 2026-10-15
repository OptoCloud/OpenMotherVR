#include "platform/win32/patching/memory.h"

#include "util/sync/once_latch.h"

#include <windows.h>

#include <expected>

namespace omvr::platform::win32 {

namespace {

namespace ports = core::diag::ports;

constexpr std::string_view kPinFailedLog     = "[hook] cannot pin the mod's module: error {}";
constexpr std::string_view kUnprotectLog     = "[hook] cannot unprotect {} bytes at {}: error {}";
constexpr std::string_view kWriteFailedLog   = "[hook] write of {} bytes at {} failed: error {}";
constexpr std::string_view kRestoreLog       = "[hook] cannot restore protection at {}: error {}";
constexpr std::string_view kReplaceFailedLog = "proxy: interception of {} failed: VirtualProtect error {}";

util::Error windows_error(std::string_view context) noexcept {
    return {.code = static_cast<std::int32_t>(::GetLastError()), .context = context};
}

} // namespace

void pin_this_module(ports::Log& log) noexcept {
    static util::OnceLatch pinned;
    if (!pinned.first()) return;
    HMODULE self = nullptr;
    if (!::GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_PIN,
                              reinterpret_cast<LPCWSTR>(&pin_this_module), &self)) {
        ports::logf(log, kPinFailedLog, ::GetLastError());
    }
}

util::Result<void> write_memory(void* dst, std::span<const std::uint8_t> bytes, ports::Log& log) noexcept {
    // Before the first write that could leave game code pointing into us.
    pin_this_module(log);
    const HANDLE self   = ::GetCurrentProcess();
    const std::size_t n = bytes.size();
    DWORD old           = 0;
    if (!::VirtualProtectEx(self, dst, n, PAGE_EXECUTE_READWRITE, &old)) {
        const auto error = windows_error("unprotect");
        ports::logf(log, kUnprotectLog, n, dst, static_cast<DWORD>(error.code));
        return std::unexpected(error);
    }
    // WriteProcessMemory flushes the instruction cache for the pages it writes.
    SIZE_T written = 0;
    util::Result<void> result;
    if (!::WriteProcessMemory(self, dst, bytes.data(), n, &written) || written != n) {
        const auto error = windows_error("write");
        ports::logf(log, kWriteFailedLog, n, dst, static_cast<DWORD>(error.code));
        result = std::unexpected(error);
    }
    // Only reached when the protection was changed, so `old` is real.
    if (DWORD unused = 0; !::VirtualProtectEx(self, dst, n, old, &unused)) {
        ports::logf(log, kRestoreLog, dst, ::GetLastError());
    }
    return result;
}

void* replace_pointer(void** slot, void* replacement, const char* method, ports::Log& log) noexcept {
    DWORD old_protect = 0;
    if (!::VirtualProtect(slot, sizeof(void*), PAGE_READWRITE, &old_protect)) {
        ports::logf(log, kReplaceFailedLog, method, ::GetLastError());
        return nullptr;
    }
    // From here the slot points into this DLL for the rest of the process.
    pin_this_module(log);
    void* prev   = *slot;
    *slot        = replacement;
    DWORD ignore = 0;
    ::VirtualProtect(slot, sizeof(void*), old_protect, &ignore);
    return prev;
}

std::uint8_t* allocate_code(std::size_t size) noexcept {
    return static_cast<std::uint8_t*>(
        ::VirtualAlloc(nullptr, size, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE));
}

void seal_code(std::uint8_t* code, std::size_t size) noexcept {
    DWORD old_protect = 0;
    ::VirtualProtect(code, size, PAGE_EXECUTE_READ, &old_protect);
    ::FlushInstructionCache(::GetCurrentProcess(), code, size);
}

} // namespace omvr::platform::win32
