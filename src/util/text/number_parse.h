#pragma once

#include <optional>

namespace omvr::util {

// A command's value: a decimal integer with an optional sign and nothing
// after it in the token. Anything else is not a value (spec 06 §4.2).
std::optional<int> parse_int(const char* s) noexcept;

// A decimal number with '.' as the decimal point whatever the system
// locale (the menu sends "1.5" and "2").
std::optional<float> parse_float(const char* s) noexcept;

// 0 or 1.
std::optional<bool> parse_switch(const char* s) noexcept;

// A decimal number with '.' as the point whatever the locale (shortcuts are
// shared between players), the whole argument; nullopt otherwise.
std::optional<float> parse_decimal(const wchar_t* arg) noexcept;

} // namespace omvr::util
