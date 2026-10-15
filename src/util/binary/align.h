// OpenMotherVR-Provenance: clean
#pragma once

#include <cstddef>

namespace omvr::util {

// `value` rounded up to a multiple of `alignment`, which must not be zero.
[[nodiscard]] constexpr std::size_t align_up(std::size_t value, std::size_t alignment) noexcept {
    return (value + alignment - 1) / alignment * alignment;
}

} // namespace omvr::util
