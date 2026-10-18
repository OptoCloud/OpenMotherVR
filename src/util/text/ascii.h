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

// `a` and `b` are equal after `fold` maps each character.
template <class Fold>
constexpr bool equals_folded(std::string_view a, std::string_view b, Fold fold) noexcept {
    if (a.size() != b.size()) return false;
    for (std::size_t i = 0; i < a.size(); ++i) {
        if (fold(a[i]) != fold(b[i])) return false;
    }
    return true;
}

// The first position of `needle` in `hay` comparing characters after
// `fold`, or npos.
template <class Fold>
constexpr std::size_t find_folded(std::string_view hay, std::string_view needle, Fold fold) noexcept {
    if (needle.size() > hay.size()) return std::string_view::npos;
    for (std::size_t at = 0; at + needle.size() <= hay.size(); ++at) {
        if (equals_folded(hay.substr(at, needle.size()), needle, fold)) return at;
    }
    return std::string_view::npos;
}

// The first position of `needle` in `hay` ignoring ASCII letter case, or npos.
constexpr std::size_t find_ignoring_ascii_case(std::string_view hay, std::string_view needle) noexcept {
    return find_folded(hay, needle, ascii_lower<char>);
}

// `needle` appears in `hay`, ignoring ASCII letter case.
constexpr bool contains_ignoring_ascii_case(std::string_view hay, std::string_view needle) noexcept {
    return find_ignoring_ascii_case(hay, needle) != std::string_view::npos;
}

} // namespace omvr::util
