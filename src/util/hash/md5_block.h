// OpenMotherVR-Provenance: clean
#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>

namespace omvr::util {

// RFC 1321 section 3: MD5 works on 64-byte blocks, starts from this state
// (A, B, C, D), and pads a message with one 0x80 byte, zeros and its
// length in bits.
inline constexpr std::size_t kMd5BlockBytes = 64;
using Md5State                              = std::array<std::uint32_t, 4>;
inline constexpr Md5State kMd5InitialState  = {0x67452301, 0xEFCDAB89, 0x98BADCFE, 0x10325476};
inline constexpr std::uint8_t kMd5Pad       = 0x80;

// The MD5 compression function (RFC 1321 section 3.4): folds one block into
// `state`. Padding and the final length block are the caller's. Pure; any
// thread.
void md5_block(Md5State& state, std::span<const std::uint8_t, kMd5BlockBytes> block) noexcept;

} // namespace omvr::util
