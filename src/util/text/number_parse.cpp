// OpenMotherVR-Provenance: clean
#include "util/text/number_parse.h"

#include <array>
#include <charconv>
#include <cmath>
#include <cstddef>
#include <string_view>
#include <system_error>
#include <type_traits>

namespace omvr::util {

namespace {

// A command-line number longer than this is not one the mod accepts; the
// bound keeps the narrowed copy on the stack.
constexpr std::size_t kMaxDecimalChars = 64;
// The last 7-bit ASCII code: a wide character above it cannot be part of a
// decimal number, and narrowing it would change it.
constexpr wchar_t kLastAscii = 0x7F;

// The one grammar of every number here: an optional '+' or '-' (one sign
// only), then digits (with a '.' for a decimal), and nothing after it in
// the token. from_chars refuses a '+' itself, so it is skipped here; a '-'
// after it would be a second sign.
template <class T>
std::optional<T> parse_whole(std::string_view s) noexcept {
    if (!s.empty() && s.front() == '+') {
        s.remove_prefix(1);
        if (!s.empty() && s.front() == '-') return std::nullopt;
    }
    if (s.empty()) return std::nullopt;
    T v = 0;
    std::from_chars_result r{};
    if constexpr (std::is_floating_point_v<T>) {
        // Fixed: no exponent; and only finite values ("nan", "inf" are not
        // numbers to a player).
        r = std::from_chars(s.data(), s.data() + s.size(), v, std::chars_format::fixed);
    } else {
        r = std::from_chars(s.data(), s.data() + s.size(), v);
    }
    if (r.ec != std::errc{} || r.ptr != s.data() + s.size()) return std::nullopt;
    if constexpr (std::is_floating_point_v<T>) {
        if (!std::isfinite(v)) return std::nullopt;
    }
    return v;
}

} // namespace

std::optional<int> parse_int(const char* s) noexcept {
    if (!s) return std::nullopt;
    return parse_whole<int>(s);
}

std::optional<float> parse_float(const char* s) noexcept {
    if (!s) return std::nullopt;
    return parse_whole<float>(s);
}

std::optional<bool> parse_switch(const char* s) noexcept {
    const auto n = parse_int(s);
    if (!n || (*n != 0 && *n != 1)) return std::nullopt;
    return *n == 1;
}

std::optional<float> parse_decimal(const wchar_t* arg) noexcept {
    if (!arg) return std::nullopt;
    std::array<char, kMaxDecimalChars> buf{};
    std::size_t n = 0;
    for (; arg[n] != L'\0'; ++n) {
        if (n == buf.size() || arg[n] > kLastAscii) return std::nullopt;
        buf[n] = static_cast<char>(arg[n]);
    }
    return parse_whole<float>(std::string_view(buf.data(), n));
}

} // namespace omvr::util
