#include "util/hash/crc32.h"

#include <array>

namespace omvr::util {

namespace {

constexpr std::array<std::uint32_t, 256> kCrcTable = [] {
    std::array<std::uint32_t, 256> t{};
    for (std::uint32_t i = 0; i < 256; ++i) {
        std::uint32_t c = i;
        for (int k = 0; k < 8; ++k) c = (c & 1) ? 0xEDB88320u ^ (c >> 1) : c >> 1;
        t[i] = c;
    }
    return t;
}();

} // namespace

std::uint32_t crc32(std::span<const std::byte> data) noexcept {
    std::uint32_t c = 0xFFFFFFFFu;
    for (std::byte b : data) c = kCrcTable[(c ^ std::to_integer<std::uint32_t>(b)) & 0xFF] ^ (c >> 8);
    return c ^ 0xFFFFFFFFu;
}

} // namespace omvr::util
