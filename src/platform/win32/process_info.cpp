#include "platform/win32/process_info.h"

#include "platform/win32/proc.h"
#include "platform/win32/text.h"

#include <windows.h>

#include <filesystem>

namespace omvr::platform::win32 {

unsigned long windows_build() noexcept {
    using RtlGetVersionFn = LONG(WINAPI*)(RTL_OSVERSIONINFOW*);
    HMODULE ntdll = ::GetModuleHandleW(L"ntdll.dll");
    const auto get = ntdll ? proc<RtlGetVersionFn>(ntdll, "RtlGetVersion") : nullptr;
    RTL_OSVERSIONINFOW v{};
    v.dwOSVersionInfoSize = sizeof(v);
    if (!get || get(&v) != 0) return 0;
    return v.dwBuildNumber;
}

const char* process_kind() noexcept {
    const bool steam = ::GetEnvironmentVariableW(L"SteamGameId", nullptr, 0) != 0
                    || ::GetEnvironmentVariableW(L"SteamAppId", nullptr, 0) != 0;
    return steam ? "Game" : "Launcher";
}

std::string process_name(std::uint32_t pid) {
    HANDLE p = ::OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
    if (!p) return {};
    wchar_t buf[MAX_PATH] = {};
    DWORD size = MAX_PATH;
    const BOOL ok = ::QueryFullProcessImageNameW(p, 0, buf, &size);
    ::CloseHandle(p);
    return ok ? utf8(std::filesystem::path{buf}.filename()) : std::string{};
}

std::uint32_t current_process_id() noexcept { return ::GetCurrentProcessId(); }

bool process_elevated() noexcept {
    HANDLE token = nullptr;
    if (!::OpenProcessToken(::GetCurrentProcess(), TOKEN_QUERY, &token)) return false;
    TOKEN_ELEVATION e{};
    DWORD size = 0;
    const BOOL ok = ::GetTokenInformation(token, TokenElevation, &e, sizeof(e), &size);
    ::CloseHandle(token);
    return ok && e.TokenIsElevated;
}

} // namespace omvr::platform::win32
