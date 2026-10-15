#pragma once

#include <array>
#include <cstddef>
#include <format>
#include <string_view>
#include <utility>

namespace omvr::util {

// std::format into a fixed buffer, truncated to fit; no allocation, so it may
// run on the render thread. The view is valid while `buf` is.
template <std::size_t N, class... Args>
std::string_view format_to(std::array<char, N>& buf, std::format_string<Args...> fmt, Args&&... args) {
    const auto r = std::format_to_n(buf.data(), N, fmt, std::forward<Args>(args)...);
    return {buf.data(), static_cast<std::size_t>(r.out - buf.data())};
}

} // namespace omvr::util
