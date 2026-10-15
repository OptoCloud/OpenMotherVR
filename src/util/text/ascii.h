// OpenMotherVR-Provenance: clean
#pragma once

#include <cstddef>
#include <string_view>

namespace omvr::util {

// The ASCII lower-case form of `c`; anything outside 'A'..'Z' unchanged.
// Independent of the C locale, so a switch or key matches the same way on
// every system.
template <class Char>
constexpr Char ascii_lower(Char c) noexcept {
    return c >= Char{'A'} && c <= Char{'Z'} ? static_cast<Char>(c - Char{'A'} + Char{'a'}) : c;
}

// `a` and `b` are equal ignoring ASCII letter case.
template <class Char>
constexpr bool equals_ignoring_ascii_case(std::basic_string_view<Char> a,
                                          std::basic_string_view<Char> b) noexcept {
    if (a.size() != b.size()) return false;
    for (std::size_t i = 0; i < a.size(); ++i) {
        if (ascii_lower(a[i]) != ascii_lower(b[i])) return false;
    }
    return true;
}

} // namespace omvr::util
