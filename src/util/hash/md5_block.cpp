// OpenMotherVR-Provenance: clean
#include "util/hash/md5_block.h"

#include <array>
#include <bit>
#include <cstring>

namespace omvr::util {

namespace {

// RFC 1321 section 3.4: 64 steps in four rounds of 16, over a block of
// sixteen 32-bit words.
constexpr int kSteps              = 64;
constexpr int kStepsPerRound      = 16;
constexpr std::size_t kBlockWords = 16;

// floor(abs(sin(i + 1)) * 2^32)
constexpr std::array<std::uint32_t, kSteps> kMd5K = {
    0xd76aa478, 0xe8c7b756, 0x242070db, 0xc1bdceee, 0xf57c0faf, 0x4787c62a, 0xa8304613, 0xfd469501,
    0x698098d8, 0x8b44f7af, 0xffff5bb1, 0x895cd7be, 0x6b901122, 0xfd987193, 0xa679438e, 0x49b40821,
    0xf61e2562, 0xc040b340, 0x265e5a51, 0xe9b6c7aa, 0xd62f105d, 0x02441453, 0xd8a1e681, 0xe7d3fbc8,
    0x21e1cde6, 0xc33707d6, 0xf4d50d87, 0x455a14ed, 0xa9e3e905, 0xfcefa3f8, 0x676f02d9, 0x8d2a4c8a,
    0xfffa3942, 0x8771f681, 0x6d9d6122, 0xfde5380c, 0xa4beea44, 0x4bdecfa9, 0xf6bb4b60, 0xbebfbc70,
    0x289b7ec6, 0xeaa127fa, 0xd4ef3085, 0x04881d05, 0xd9d4d039, 0xe6db99e5, 0x1fa27cf8, 0xc4ac5665,
    0xf4292244, 0x432aff97, 0xab9423a7, 0xfc93a039, 0x655b59c3, 0x8f0ccc92, 0xffeff47d, 0x85845dd1,
    0x6fa87e4f, 0xfe2ce6e0, 0xa3014314, 0x4e0811a1, 0xf7537e82, 0xbd3af235, 0x2ad7d2bb, 0xeb86d391,
};

// The left-rotate amounts per step (RFC 1321 section 3.4).
constexpr std::array<int, kSteps> kMd5S = {
    7,  12, 17, 22, 7,  12, 17, 22, 7,  12, 17, 22, 7,  12, 17, 22, 5,  9,  14, 20, 5,  9,
    14, 20, 5,  9,  14, 20, 5,  9,  14, 20, 4,  11, 16, 23, 4,  11, 16, 23, 4,  11, 16, 23,
    4,  11, 16, 23, 6,  10, 15, 21, 6,  10, 15, 21, 6,  10, 15, 21, 6,  10, 15, 21,
};

// The message word each round reads at step i: i, 5i + 1, 3i + 5 and 7i,
// all modulo 16 (RFC 1321 section 3.4).
struct WordOrder {
    int scale;
    int offset;
};
constexpr std::array<WordOrder, kSteps / kStepsPerRound> kWordOrder = {{{1, 0}, {5, 1}, {3, 5}, {7, 0}}};

// The round's auxiliary function F, G, H or I of b, c, d.
constexpr std::uint32_t round_function(int round, std::uint32_t b, std::uint32_t c,
                                       std::uint32_t d) noexcept {
    switch (round) {
        case 0: return (b & c) | (~b & d);
        case 1: return (d & b) | (~d & c);
        case 2: return b ^ c ^ d;
        default: return c ^ (b | ~d);
    }
}

} // namespace

void md5_block(std::uint32_t state[4], const std::uint8_t* block) noexcept {
    std::array<std::uint32_t, kBlockWords> m{};
    std::memcpy(m.data(), block, sizeof(m));
    std::uint32_t a = state[0], b = state[1], c = state[2], d = state[3];
    for (int i = 0; i < kSteps; ++i) {
        const int round   = i / kStepsPerRound;
        const auto& order = kWordOrder[static_cast<std::size_t>(round)];
        const auto g      = static_cast<std::size_t>((order.scale * i + order.offset) % kStepsPerRound);
        const std::uint32_t f =
            round_function(round, b, c, d) + a + kMd5K[static_cast<std::size_t>(i)] + m[g];
        a = d;
        d = c;
        c = b;
        b += std::rotl(f, kMd5S[static_cast<std::size_t>(i)]);
    }
    state[0] += a;
    state[1] += b;
    state[2] += c;
    state[3] += d;
}

} // namespace omvr::util
