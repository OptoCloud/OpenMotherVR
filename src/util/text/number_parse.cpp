#include "util/text/number_parse.h"

#include <charconv>
#include <cstddef>
#include <cstring>
#include <system_error>

namespace omvr::util {

std::optional<int> parse_int(const char* s) noexcept {
    if (!s) return std::nullopt;
    if (*s == '+') ++s;
    const char* end = s + std::strlen(s);
    int v = 0;
    const auto [p, ec] = std::from_chars(s, end, v);
    if (ec != std::errc{} || p != end || p == s) return std::nullopt;
    return v;
}

std::optional<float> parse_float(const char* s) noexcept {
    if (!s) return std::nullopt;
    if (*s == '+') ++s;
    const char* end = s + std::strlen(s);
    float v = 0.0f;
    const auto [p, ec] = std::from_chars(s, end, v, std::chars_format::fixed);
    if (ec != std::errc{} || p != end || p == s) return std::nullopt;
    return v;
}

std::optional<bool> parse_switch(const char* s) noexcept {
    const auto n = parse_int(s);
    if (!n || (*n != 0 && *n != 1)) return std::nullopt;
    return *n == 1;
}

std::optional<float> parse_decimal(const wchar_t* arg) noexcept {
    char buf[64];
    std::size_t n = 0;
    for (; arg[n] != L'\0'; ++n) {
        if (n == sizeof(buf) || arg[n] > 0x7F) return std::nullopt;
        buf[n] = static_cast<char>(arg[n]);
    }
    float value = 0.0f;
    const auto [end, ec] = std::from_chars(buf, buf + n, value);
    if (n == 0 || ec != std::errc{} || end != buf + n) return std::nullopt;
    return value;
}

} // namespace omvr::util
