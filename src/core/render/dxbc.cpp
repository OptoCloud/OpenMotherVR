#include "core/render/dxbc.h"

#include "core/render/dxbc_tokens.h"
#include "util/hash/md5_block.h"

#include <cstring>

namespace omvr::core::render {

using util::md5_block;

// Parses the operand at `t[i]`; only the simple forms these shaders use.
std::optional<Operand> parse_operand(const std::uint32_t* t, std::size_t end, std::size_t i) {
    if (i >= end) return std::nullopt;
    const std::uint32_t tok = t[i];
    Operand o{i, 1, op_type(tok), 0};
    std::size_t p = i + 1;
    if (op_extended(tok)) ++p;                       // one modifier token (negate, abs)
    if (o.type == kTypeImmediate32) {
        const std::uint32_t comps = (tok & 0x3) == 2 ? 4 : 1;
        p += comps;
    } else {
        const std::uint32_t dims = index_dim(tok);
        for (std::uint32_t d = 0; d < dims; ++d) {
            const std::uint32_t rep = (tok >> (22 + 3 * d)) & 0x7;
            if (rep != 0) return std::nullopt;       // only immediate32 indices
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
    if (o.type != kTypeImmediate32 || o.size != 5) return false;
    return t[o.at + 1] == x && t[o.at + 2] == y && t[o.at + 3] == z && t[o.at + 4] == w;
}

void dxbc_checksum(const std::uint8_t* blob, std::size_t total, std::uint32_t out[4]) noexcept {
    const std::uint8_t* data = blob + 20;
    const std::size_t size = total - 20;
    std::uint32_t state[4] = {0x67452301, 0xEFCDAB89, 0x98BADCFE, 0x10325476};
    const std::size_t full = size - size % 64;
    for (std::size_t off = 0; off < full; off += 64) md5_block(state, data + off);
    const std::size_t left = size - full;
    const std::uint32_t bits = static_cast<std::uint32_t>(size * 8);
    const std::uint32_t bits_part = (bits >> 2) | 1;
    std::uint8_t block[64]{};
    if (left >= 56) {
        std::memcpy(block, data + full, left);
        block[left] = 0x80;
        md5_block(state, block);
        std::memset(block, 0, 64);
        std::memcpy(block, &bits, 4);
        std::memcpy(block + 60, &bits_part, 4);
    } else {
        std::memcpy(block, &bits, 4);
        std::memcpy(block + 4, data + full, left);
        block[4 + left] = 0x80;
        std::memcpy(block + 60, &bits_part, 4);
    }
    md5_block(state, block);
    std::memcpy(out, state, 16);
}

} // namespace omvr::core::render
