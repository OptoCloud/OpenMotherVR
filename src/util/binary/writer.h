#pragma once

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <vector>

namespace omvr::util {

// Writes a little-endian value at byte `at` of `out`, which must be large
// enough.
template <class T>
void put_at(std::vector<std::byte>& out, std::size_t at, T value) noexcept {
    std::memcpy(out.data() + at, &value, sizeof(T));
}

// Writes `value` as a u32 at byte `at`.
inline void put_u32_at(std::vector<std::byte>& out, std::size_t at, std::uint32_t value) noexcept {
    put_at(out, at, value);
}

} // namespace omvr::util
