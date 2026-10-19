// OpenMotherVR-Provenance: clean
#pragma once

#include <optional>

namespace omvr::util {

// The parsers share one grammar: an optional single sign, digits, for a
// decimal a '.' as the point whatever the system locale and no exponent,
// a finite value, and nothing else in the token. Anything else is not a
// value (spec 06 §4.2, §2).

// A command's value: a decimal integer.
[[nodiscard]] std::optional<int> parse_int(const char* s) noexcept;

// A command's decimal number (the menu sends "1.5" and "2").
[[nodiscard]] std::optional<float> parse_float(const char* s) noexcept;

// 0 or 1, as parse_int reads them.
[[nodiscard]] std::optional<bool> parse_switch(const char* s) noexcept;

// A command-line decimal number (shortcuts are shared between players, so
// the locale must not matter), the whole argument; nullopt otherwise, or
// for a character outside ASCII.
[[nodiscard]] std::optional<float> parse_decimal(const wchar_t* arg) noexcept;

} // namespace omvr::util
