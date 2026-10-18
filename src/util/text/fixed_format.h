// OpenMotherVR-Provenance: clean
#pragma once

#include <cstddef>
#include <format>
#include <span>
#include <string_view>
#include <utility>

namespace omvr::util {

// std::format into `out`, truncated to fit; no allocation, so it may run on
// the render thread. Returns what was written, valid while `out` is. Pass
// a buffer one shorter to keep room for a terminating null.
template <class... Args>
std::string_view format_into(std::span<char> out, std::format_string<Args...> fmt, Args&&... args) {
    const auto r = std::format_to_n(out.data(), static_cast<std::ptrdiff_t>(out.size()), fmt,
                                    std::forward<Args>(args)...);
    return {out.data(), static_cast<std::size_t>(r.out - out.data())};
}

} // namespace omvr::util
