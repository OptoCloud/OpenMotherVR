#include "platform/win32/registry.h"

#include <windows.h>

#include <cwchar>
#include <utility>

namespace omvr::platform::win32 {

namespace {

REGSAM view_flag(RegistryView view) noexcept {
    switch (view) {
        case RegistryView::Bits32: return KEY_WOW64_32KEY;
        case RegistryView::Bits64: return KEY_WOW64_64KEY;
        case RegistryView::Default: break;
    }
    return 0;
}

bool is_string(DWORD type) noexcept { return type == REG_SZ || type == REG_EXPAND_SZ; }

// `value` with its environment references expanded, or as it is if that
// fails.
std::wstring expanded(std::wstring value) {
    if (value.empty()) return value;
    const DWORD needed = ::ExpandEnvironmentStringsW(value.c_str(), nullptr, 0);
    if (needed == 0) return value;
    std::wstring out(needed, L'\0');
    const DWORD got = ::ExpandEnvironmentStringsW(value.c_str(), out.data(), needed);
    if (got == 0 || got > needed) return value;
    out.resize(got - 1); // without the terminator
    return out;
}

} // namespace

std::optional<std::wstring> read_machine_string(std::wstring_view subkey, std::wstring_view name,
                                                RegistryView view) noexcept {
    try {
        const std::wstring key_name(subkey);
        const std::wstring value_name(name);
        HKEY key = nullptr;
        if (::RegOpenKeyExW(HKEY_LOCAL_MACHINE, key_name.c_str(), 0, KEY_READ | view_flag(view), &key) !=
            ERROR_SUCCESS) {
            return std::nullopt;
        }
        // Ask for the size first instead of assuming MAX_PATH.
        DWORD type     = 0;
        DWORD size     = 0;
        LSTATUS status = ::RegQueryValueExW(key, value_name.c_str(), nullptr, &type, nullptr, &size);
        std::wstring value;
        if (status == ERROR_SUCCESS && is_string(type) && size > 0) {
            value.resize(size / sizeof(wchar_t) + 1); // room for a missing terminator
            auto got = static_cast<DWORD>(value.size() * sizeof(wchar_t));
            status   = ::RegQueryValueExW(key, value_name.c_str(), nullptr, &type,
                                          reinterpret_cast<BYTE*>(value.data()), &got);
        }
        ::RegCloseKey(key);
        if (status != ERROR_SUCCESS || !is_string(type)) return std::nullopt;
        value.resize(::wcsnlen(value.c_str(), value.size()));
        return type == REG_EXPAND_SZ ? expanded(std::move(value)) : value;
    } catch (...) {
        return std::nullopt;
    }
}

} // namespace omvr::platform::win32
