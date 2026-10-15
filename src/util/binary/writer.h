#pragma once

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <type_traits>
#include <vector>

namespace omvr::util {

// Writes `value` as stored (little-endian on the x86 target) at byte `at`
// of `out`, which must be large enough.
template <class T>
void put_at(std::vector<std::byte>& out, std::size_t at, T value) noexcept {
    static_assert(std::is_trivially_copyable_v<T>);
    std::memcpy(out.data() + at, &value, sizeof(T));
}

// Writes `value` as a u32 at byte `at`.
inline void put_u32_at(std::vector<std::byte>& out, std::size_t at, std::uint32_t value) noexcept {
    put_at(out, at, value);
}

} // namespace omvr::util
