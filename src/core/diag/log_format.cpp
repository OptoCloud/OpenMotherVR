#include "core/diag/log_format.h"

#include <algorithm>
#include <format>

namespace omvr::core::diag {

// The pid matters because AI.exe spawns a launcher and a game process and
// both run the DLL; without it the log cannot tell them apart.
std::size_t format_prefix(std::span<char> out, const ports::LocalTime& t, std::uint32_t pid) noexcept {
    const auto r = std::format_to_n(
        out.data(), static_cast<std::ptrdiff_t>(out.size()), "[{:04}-{:02}-{:02} {:02}:{:02}:{:02}.{:03} pid={:05}] ",
        static_cast<unsigned>(t.year), static_cast<unsigned>(t.month), static_cast<unsigned>(t.day),
        static_cast<unsigned>(t.hour), static_cast<unsigned>(t.minute), static_cast<unsigned>(t.second),
        static_cast<unsigned>(t.millisecond), static_cast<unsigned long>(pid));
    return static_cast<std::size_t>(r.out - out.data());
}

std::size_t format_line(std::span<char> out, const ports::LocalTime& t, std::uint32_t pid,
                        std::string_view msg) noexcept {
    if (out.size() <= kLineEnding.size()) return 0;
    const std::size_t prefix = format_prefix(out, t, pid);
    if (prefix >= out.size() - kLineEnding.size()) return 0;
    const std::size_t room = out.size() - prefix - kLineEnding.size();
    const std::string_view body = msg.substr(0, std::min(msg.size(), room));
    auto end = std::ranges::copy(body, out.begin() + static_cast<std::ptrdiff_t>(prefix)).out;
    std::ranges::copy(kLineEnding, end);
    return prefix + body.size() + kLineEnding.size();
}

// path::string() would use the ANSI code page and fail on a user name
// outside it; UTF-8 holds any path.
std::string path_text(const std::filesystem::path& path) noexcept {
    try {
        const std::u8string text = path.u8string();
        // char may alias any object, so the UTF-8 bytes are read as chars.
        return std::string(reinterpret_cast<const char*>(text.data()), text.size());
    } catch (...) {
        return {};
    }
}

} // namespace omvr::core::diag
