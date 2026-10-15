#pragma once

#include <cstdint>

namespace omvr::util {

// The MD5 compression function (RFC 1321 section 3.4): folds one 64-byte
// block into the four-word `state`. Padding and the final length block are
// the caller's. Pure; any thread.
void md5_block(std::uint32_t state[4], const std::uint8_t* block) noexcept;

} // namespace omvr::util
