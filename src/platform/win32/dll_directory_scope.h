#pragma once

#include <filesystem>
#include <string>

namespace omvr::platform::win32 {

// Points the process DLL search path at `dir` and puts back whatever was
// set before (by the game or an overlay) when it goes out of scope. The
// search path is process-wide, so keep the scope short.
class DllDirectoryScope {
public:
    explicit DllDirectoryScope(const std::filesystem::path& dir) noexcept;
    ~DllDirectoryScope();
    DllDirectoryScope(const DllDirectoryScope&) = delete;
    DllDirectoryScope& operator=(const DllDirectoryScope&) = delete;
    DllDirectoryScope(DllDirectoryScope&&) = delete;
    DllDirectoryScope& operator=(DllDirectoryScope&&) = delete;

private:
    std::wstring previous_;
};

} // namespace omvr::platform::win32
