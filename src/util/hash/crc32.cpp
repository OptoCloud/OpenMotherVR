// OpenMotherVR-Provenance: clean
#include "util/hash/crc32.h"

#include <array>
#include <cstddef>

namespace omvr::util {

namespace {

// zlib's CRC-32: the reflected form of the IEEE 802.3 polynomial, a register
// preset to all ones and the result inverted (RFC 1952 section 8).
constexpr std::uint32_t kPolynomial = 0xEDB88320u;
constexpr std::uint32_t kPreset     = 0xFFFFFFFFu;
constexpr std::uint32_t kFinalXor   = 0xFFFFFFFFu;

constexpr std::size_t kByteValues = 256;
constexpr int kBitsPerByte        = 8;
constexpr std::uint32_t kLowByte  = 0xFFu;

// The remainder of one byte value, one bit at a time.
constexpr std::uint32_t byte_remainder(std::uint32_t value) noexcept {
    for (int bit = 0; bit < kBitsPerByte; ++bit) {
        value = (value & 1u) ? kPolynomial ^ (value >> 1) : value >> 1;
    }
    return value;
}

constexpr std::array<std::uint32_t, kByteValues> kCrcTable = [] {
    std::array<std::uint32_t, kByteValues> table{};
    for (std::size_t i = 0; i < kByteValues; ++i) table[i] = byte_remainder(static_cast<std::uint32_t>(i));
    return table;
}();

} // namespace

std::uint32_t crc32(std::span<const std::byte> data) noexcept {
    std::uint32_t crc = kPreset;
    for (const std::byte b : data) {
        crc = kCrcTable[(crc ^ std::to_integer<std::uint32_t>(b)) & kLowByte] ^ (crc >> kBitsPerByte);
    }
    return crc ^ kFinalXor;
}

} // namespace omvr::util
