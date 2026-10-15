// OpenMotherVR-Provenance: clean
#include "platform/win32/dll_directory_scope.h"

#include <windows.h>

namespace omvr::platform::win32 {

namespace {

// The search directory set now, or empty when none is (a length of one is
// the terminator alone).
std::wstring current_dll_directory() noexcept {
    try {
        std::wstring dir;
        if (const DWORD n = ::GetDllDirectoryW(0, nullptr); n > 1) {
            dir.resize(n);
            dir.resize(::GetDllDirectoryW(n, dir.data()));
        }
        return dir;
    } catch (...) {
        return {};
    }
}

} // namespace

DllDirectoryScope::DllDirectoryScope(const std::filesystem::path& dir) noexcept
    : previous_(current_dll_directory()) {
    ::SetDllDirectoryW(dir.c_str());
}

DllDirectoryScope::~DllDirectoryScope() {
    ::SetDllDirectoryW(previous_.empty() ? nullptr : previous_.c_str());
}

} // namespace omvr::platform::win32
