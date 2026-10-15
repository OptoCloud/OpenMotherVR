// OpenMotherVR-Provenance: clean
#include "core/render/dxbc.h"

#include "core/render/dxbc_tokens.h"
#include "util/hash/md5_block.h"

#include <cstring>

namespace omvr::core::render {

namespace {

// RFC 1321 §3.3: MD5's initial state.
constexpr DxbcChecksum kMd5InitialState = {0x67452301, 0xEFCDAB89, 0x98BADCFE, 0x10325476};
constexpr std::size_t kMd5BlockBytes    = 64;
constexpr std::uint8_t kMd5Pad          = 0x80;
// DXBC's final block: the length in bits first, the rest of the data and
// the pad after it; a remainder that leaves no room for that (56 bytes or
// more) gets a block of its own and the length goes in a block of zeros.
// The last word is the length in bits over 4, with bit 0 set.
constexpr std::size_t kRoomForLength  = 56;
constexpr std::size_t kLastWordOffset = 60;
constexpr std::uint32_t kBitsPerByte  = 8;

constexpr std::size_t kImmediateValues4 = 4;
constexpr std::size_t kImmediateValues1 = 1;

} // namespace

std::optional<Operand> parse_operand(const std::uint32_t* t, std::size_t end, std::size_t i) {
    if (i >= end) return std::nullopt;
    const std::uint32_t tok = t[i];
    Operand o{i, 1, op_type(tok), 0};
    std::size_t p = i + 1;
    if (op_extended(tok)) ++p; // one modifier token (negate, abs)
    if (o.type == kTypeImmediate32) {
        p += (tok & kComponentCountMask) == kFourComponents ? kImmediateValues4 : kImmediateValues1;
    } else {
        const std::uint32_t dims = index_dim(tok);
        for (std::uint32_t d = 0; d < dims; ++d) {
            if (index_rep(tok, d) != kIndexImmediate32) return std::nullopt;
            if (d == 0) o.reg = t[p];
            ++p;
        }
    }
    if (p > end) return std::nullopt;
    o.size = p - i;
    return o;
}

bool is_immediate4(const std::uint32_t* t, const Operand& o, std::uint32_t x, std::uint32_t y,
                   std::uint32_t z, std::uint32_t w) {
    if (o.type != kTypeImmediate32 || o.size != 1 + kImmediateValues4) return false;
    return t[o.at + 1] == x && t[o.at + 2] == y && t[o.at + 3] == z && t[o.at + 4] == w;
}

DxbcChecksum dxbc_checksum(std::span<const std::uint8_t> blob) noexcept {
    const std::uint8_t* data = blob.data() + kChecksummedFrom;
    const std::size_t size   = blob.size() - kChecksummedFrom;
    DxbcChecksum state       = kMd5InitialState;
    const std::size_t full   = size - size % kMd5BlockBytes;
    for (std::size_t off = 0; off < full; off += kMd5BlockBytes) util::md5_block(state.data(), data + off);
    const std::size_t left        = size - full;
    const std::uint32_t bits      = static_cast<std::uint32_t>(size * kBitsPerByte);
    const std::uint32_t last_word = (bits >> 2) | 1;
    std::uint8_t block[kMd5BlockBytes]{};
    if (left >= kRoomForLength) {
        std::memcpy(block, data + full, left);
        block[left] = kMd5Pad;
        util::md5_block(state.data(), block);
        std::memset(block, 0, sizeof(block));
        std::memcpy(block, &bits, sizeof(bits));
        std::memcpy(block + kLastWordOffset, &last_word, sizeof(last_word));
    } else {
        std::memcpy(block, &bits, sizeof(bits));
        std::memcpy(block + sizeof(bits), data + full, left);
        block[sizeof(bits) + left] = kMd5Pad;
        std::memcpy(block + kLastWordOffset, &last_word, sizeof(last_word));
    }
    util::md5_block(state.data(), block);
    return state;
}

} // namespace omvr::core::render
