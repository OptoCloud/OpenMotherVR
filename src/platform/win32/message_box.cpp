#include "platform/win32/message_box.h"

#include <windows.h>

#include <string>

namespace omvr::platform::win32 {

namespace {

// `text` (UTF-8) as UTF-16 for MessageBoxW.
std::wstring wide(std::string_view text) {
    const int n = ::MultiByteToWideChar(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), nullptr, 0);
    std::wstring out(static_cast<std::size_t>(n), L'\0');
    ::MultiByteToWideChar(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), out.data(), n);
    return out;
}

// The log line that carries the box's text. The kind names the message for
// a tester searching the log; a box without one keeps the plain prefix.
std::string log_line_for(std::string_view text, const char* kind) {
    std::string line =
        kind ? std::string{"message box ("}.append(kind).append("): ") : std::string{"message box: "};
    return line.append(text);
}

} // namespace

int message_box(core::diag::ports::Log& log, std::string_view text, unsigned type, const char* kind,
                const wchar_t* caption) noexcept {
    try {
        log.line(log_line_for(text, kind));
        // Kept alive past GetLastError: freeing it could overwrite the error.
        const std::wstring body = wide(text);
        const int pressed       = ::MessageBoxW(nullptr, body.c_str(), caption, type);
        // The text is in the log already; record that the player never saw it.
        if (pressed == 0)
            core::diag::ports::logf(log, "message box could not be shown err=0x{:08X}", ::GetLastError());
        return pressed;
    } catch (...) {
        log.line("message box could not be shown: out of memory");
        return 0;
    }
}

} // namespace omvr::platform::win32
