#pragma once

#include <windows.h>
#include <filesystem>
#include <format>
#include <string>
#include <string_view>

namespace mothervr::diag {

// The directory holding this DLL (the game's install directory); empty if it
// cannot be determined. Works before init().
std::filesystem::path module_directory() noexcept;

// A path as UTF-8, for logs and for APIs that take UTF-8 (OpenVR). Never
// throws, unlike path::string(), which fails on characters outside the ANSI
// code page (a user name, an install path).
std::string utf8(const std::filesystem::path& path) noexcept;

// Initialize the logger. Resolves the log path to `<dir-of-this-DLL>/mothervr.log`
// using `self` (the DLL's HINSTANCE) and truncates any existing log so each
// process attach starts fresh. Call once from DllMain DLL_PROCESS_ATTACH.
//
// All log_* calls before init() are no-ops (safe).
void init(HINSTANCE self) noexcept;

// Append one line. Prefixed with timestamp + pid, line-terminated with \r\n.
void log_line(std::string_view msg) noexcept;

// std::format-style. Bounded to ~1 KB per line; longer messages are truncated.
template <class... Args>
inline void logf(std::format_string<Args...> fmt, Args&&... args) {
    char buf[1024];
    auto r = std::format_to_n(buf, sizeof(buf) - 1, fmt, std::forward<Args>(args)...);
    log_line(std::string_view{buf, static_cast<std::size_t>(r.out - buf)});
}

// A message box captioned "MotherVR" for the cases the original reports to
// the player (first launch, unknown game build, VR runtime failures). `text`
// is UTF-8; `type` is MB_* flags. Returns the button pressed.
// From: FUN_1001d540 / FUN_1001d5b0.
int message_box(std::string_view text, unsigned type) noexcept;

} // namespace mothervr::diag
