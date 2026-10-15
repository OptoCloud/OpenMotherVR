#include "platform/win32/text.h"

#include <windows.h>

namespace omvr::platform::win32 {

std::string utf8(const std::filesystem::path& path) noexcept {
    try {
        const std::wstring& w = path.native();
        const int n = ::WideCharToMultiByte(CP_UTF8, 0, w.data(), static_cast<int>(w.size()), nullptr, 0,
                                            nullptr, nullptr);
        std::string out(static_cast<std::size_t>(n), '\0');
        ::WideCharToMultiByte(CP_UTF8, 0, w.data(), static_cast<int>(w.size()), out.data(), n, nullptr,
                              nullptr);
        return out;
    } catch (...) {
        return {};
    }
}

} // namespace omvr::platform::win32
