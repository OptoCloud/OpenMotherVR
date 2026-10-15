#pragma once

#include <windows.h>

#include <filesystem>
#include <string>

namespace omvr::platform::win32 {

// Points the process DLL search path at `dir` and puts back whatever was
// set before (by the game or an overlay) when it goes out of scope. The
// search path is process-wide, so keep the scope short.
class DllDirectoryScope {
public:
    explicit DllDirectoryScope(const std::filesystem::path& dir) noexcept {
        try {
            if (const DWORD n = ::GetDllDirectoryW(0, nullptr); n > 1) {
                previous_.resize(n);
                previous_.resize(::GetDllDirectoryW(n, previous_.data()));
            }
        } catch (...) {
            previous_.clear();
        }
        ::SetDllDirectoryW(dir.c_str());
    }
    ~DllDirectoryScope() { ::SetDllDirectoryW(previous_.empty() ? nullptr : previous_.c_str()); }
    DllDirectoryScope(const DllDirectoryScope&) = delete;
    DllDirectoryScope& operator=(const DllDirectoryScope&) = delete;

private:
    std::wstring previous_;
};

} // namespace omvr::platform::win32
