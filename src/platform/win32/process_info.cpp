#include "platform/win32/process_info.h"

#include "platform/win32/constants.h"
#include "platform/win32/environment.h"
#include "platform/win32/proc.h"
#include "platform/win32/text.h"
#include "platform/win32/unique_handle.h"

#include <windows.h>

#include <filesystem>

namespace omvr::platform::win32 {

unsigned long windows_build() noexcept {
    using RtlGetVersionFn = LONG(WINAPI*)(RTL_OSVERSIONINFOW*);
    HMODULE ntdll         = ::GetModuleHandleW(kNtdllFileName);
    const auto get        = ntdll ? proc<RtlGetVersionFn>(ntdll, kRtlGetVersionName) : nullptr;
    RTL_OSVERSIONINFOW v{};
    v.dwOSVersionInfoSize = sizeof(v);
    if (!get || get(&v) != 0) return 0; // STATUS_SUCCESS is 0
    return v.dwBuildNumber;
}

const char* process_kind() noexcept {
    const bool steam =
        has_environment_variable(kSteamGameIdVariable) || has_environment_variable(kSteamAppIdVariable);
    return steam ? "Game" : "Launcher";
}

std::string process_name(std::uint32_t pid) {
    const UniqueHandle process{::OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid)};
    if (!process) return {};
    wchar_t buf[MAX_PATH] = {};
    DWORD size            = MAX_PATH;
    if (!::QueryFullProcessImageNameW(process.get(), 0, buf, &size)) return {};
    return utf8(std::filesystem::path{buf}.filename());
}

std::uint32_t current_process_id() noexcept { return ::GetCurrentProcessId(); }

bool process_elevated() noexcept {
    HANDLE raw = nullptr;
    if (!::OpenProcessToken(::GetCurrentProcess(), TOKEN_QUERY, &raw)) return false;
    const UniqueHandle token{raw};
    TOKEN_ELEVATION e{};
    DWORD size = 0;
    return ::GetTokenInformation(token.get(), TokenElevation, &e, sizeof(e), &size) && e.TokenIsElevated;
}

} // namespace omvr::platform::win32
