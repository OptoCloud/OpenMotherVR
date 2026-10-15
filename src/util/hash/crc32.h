#pragma once

#include <cstddef>
#include <cstdint>
#include <span>

namespace omvr::util {

// CRC-32 as zlib computes it (reflected, polynomial 0xEDB88320).
std::uint32_t crc32(std::span<const std::byte> data) noexcept;

} // namespace omvr::util
