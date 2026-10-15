#include "platform/win32/module.h"

#include "platform/win32/constants.h"

#include <windows.h>

#include <cstdint>
#include <cstring>
#include <format>

namespace omvr::platform::win32 {

std::filesystem::path module_directory() noexcept {
    HMODULE self = nullptr;
    if (!::GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                              reinterpret_cast<LPCWSTR>(&module_directory), &self))
        return {};
    try {
        std::wstring buf(MAX_PATH, L'\0');
        for (;;) {
            const DWORD n = ::GetModuleFileNameW(self, buf.data(), static_cast<DWORD>(buf.size()));
            if (n == 0) return {};
            if (n < buf.size()) {
                buf.resize(n);
                return std::filesystem::path{buf}.parent_path();
            }
            buf.resize(buf.size() * kPathBufferGrowth);
        }
    } catch (...) {
        return {};
    }
}

// The fixed part of the resource is found by its signature; no version.dll.
std::string version_string(void* module) {
    const auto self = static_cast<HINSTANCE>(module);
    HRSRC info = ::FindResourceW(self, MAKEINTRESOURCEW(1), RT_VERSION);
    HGLOBAL data = info ? ::LoadResource(self, info) : nullptr;
    const auto* bytes = data ? static_cast<const std::uint8_t*>(::LockResource(data)) : nullptr;
    const DWORD size = bytes ? ::SizeofResource(self, info) : 0;
    for (DWORD at = 0; at + sizeof(VS_FIXEDFILEINFO) <= size; at += 4) {
        VS_FIXEDFILEINFO fixed{};
        std::memcpy(&fixed, bytes + at, sizeof(fixed));
        if (fixed.dwSignature == 0xFEEF04BD) {
            return std::format("{}.{}.{}", HIWORD(fixed.dwFileVersionMS), LOWORD(fixed.dwFileVersionMS),
                               HIWORD(fixed.dwFileVersionLS));
        }
    }
    return "unknown";
}

void* module_handle(const char* name) noexcept { return ::GetModuleHandleA(name); }

} // namespace omvr::platform::win32
