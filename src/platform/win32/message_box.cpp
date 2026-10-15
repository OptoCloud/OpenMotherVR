#include "platform/win32/message_box.h"

#include "core/diag/logger.h" // R: diag (A4): log through an injected Log&

#include <windows.h>

#include <string>

namespace omvr::platform::win32 {

using core::diag::log_line;
using core::diag::logf;

int message_box(std::string_view text, unsigned type, const char* kind, const wchar_t* caption) noexcept {
    try {
        // The kind names the message for a tester searching the log; a box
        // without one keeps the plain prefix.
        std::string line = kind ? std::string{"message box ("}.append(kind).append("): ")
                                : std::string{"message box: "};
        log_line(line.append(text));
        const int n = ::MultiByteToWideChar(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), nullptr, 0);
        std::wstring wide(static_cast<std::size_t>(n), L'\0');
        ::MultiByteToWideChar(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), wide.data(), n);
        const int pressed = ::MessageBoxW(nullptr, wide.c_str(), caption, type);
        // The text is in the log already; record that the player never saw it.
        if (pressed == 0) logf("message box could not be shown err=0x{:08X}", ::GetLastError());
        return pressed;
    } catch (...) {
        log_line("message box could not be shown: out of memory");
        return 0;
    }
}

} // namespace omvr::platform::win32
