#pragma once

#include <cstdint>

namespace omvr::util {

// The MD5 compression function: folds one 64-byte block into `state`.
void md5_block(std::uint32_t state[4], const std::uint8_t* block) noexcept;

} // namespace omvr::util
