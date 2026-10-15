#include "platform/win32/environment.h"

#include <windows.h>

#include <cstdlib>
#include <memory>

namespace omvr::platform::win32 {

namespace {

struct CrtFree {
    void operator()(wchar_t* p) const noexcept { std::free(p); }
};

} // namespace

std::optional<std::wstring> environment_variable(std::wstring_view name) {
    const std::wstring key(name);
    DWORD n = ::GetEnvironmentVariableW(key.c_str(), nullptr, 0);
    if (n == 0) return std::nullopt;
    std::wstring value(n, L'\0');
    n = ::GetEnvironmentVariableW(key.c_str(), value.data(), static_cast<DWORD>(value.size()));
    if (n == 0) return std::nullopt;
    value.resize(n);
    return value;
}

bool has_environment_variable(std::wstring_view name) noexcept {
    try {
        const std::wstring key(name);
        return ::GetEnvironmentVariableW(key.c_str(), nullptr, 0) != 0;
    } catch (...) {
        return false;
    }
}

std::optional<std::wstring> crt_environment_variable(std::wstring_view name) {
    const std::wstring key(name);
    wchar_t* raw = nullptr;
    if (::_wdupenv_s(&raw, nullptr, key.c_str()) != 0 || !raw) return std::nullopt;
    const std::unique_ptr<wchar_t, CrtFree> owned{raw};
    return std::wstring{owned.get()};
}

bool set_crt_environment_variable(std::wstring_view name, std::wstring_view value) noexcept {
    try {
        const std::wstring key(name);
        const std::wstring text(value);
        return ::_wputenv_s(key.c_str(), text.c_str()) == 0;
    } catch (...) {
        return false;
    }
}

} // namespace omvr::platform::win32
