// OpenMotherVR-Provenance: clean
#include "platform/win32/module.h"

#include "platform/win32/constants.h"

#include <windows.h>

#include <cstdint>
#include <cstring>
#include <format>
#include <optional>
#include <span>

namespace omvr::platform::win32 {

namespace {

// The version resource of `module`, empty when it has none.
std::span<const std::uint8_t> version_resource(HINSTANCE module) noexcept {
    HRSRC info        = ::FindResourceW(module, MAKEINTRESOURCEW(kVersionResourceId), RT_VERSION);
    HGLOBAL data      = info ? ::LoadResource(module, info) : nullptr;
    const auto* bytes = data ? static_cast<const std::uint8_t*>(::LockResource(data)) : nullptr;
    if (!bytes) return {};
    return {bytes, ::SizeofResource(module, info)};
}

// The fixed part of a version resource, found by its signature, so no
// version.dll is needed.
std::optional<VS_FIXEDFILEINFO> fixed_file_info(std::span<const std::uint8_t> resource) noexcept {
    for (std::size_t at = 0; at + sizeof(VS_FIXEDFILEINFO) <= resource.size(); at += kVersionInfoAlignment) {
        VS_FIXEDFILEINFO fixed{};
        std::memcpy(&fixed, resource.data() + at, sizeof(fixed));
        if (fixed.dwSignature == VS_FFI_SIGNATURE) return fixed;
    }
    return std::nullopt;
}

} // namespace

std::filesystem::path module_directory() noexcept {
    HMODULE self = nullptr;
    // Any address inside this DLL names it; the function's own will do.
    if (!::GetModuleHandleExW(
            GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
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

std::string version_string(void* module) {
    if (const auto fixed = fixed_file_info(version_resource(static_cast<HINSTANCE>(module)))) {
        return std::format("{}.{}.{}", HIWORD(fixed->dwFileVersionMS), LOWORD(fixed->dwFileVersionMS),
                           HIWORD(fixed->dwFileVersionLS));
    }
    return "unknown";
}

void* module_handle(const char* name) noexcept { return ::GetModuleHandleA(name); }

void* proc_address(void* module, const char* name) noexcept {
    return reinterpret_cast<void*>(::GetProcAddress(static_cast<HMODULE>(module), name));
}

} // namespace omvr::platform::win32
