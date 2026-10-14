#include "shader_patch.h"

#include <bit>
#include <cstring>
#include <algorithm>
#include <iterator>
#include <optional>

namespace mothervr::render {

namespace {

// ---- DXBC checksum -----------------------------------------------------------

// floor(abs(sin(i + 1)) * 2^32)
constexpr std::uint32_t kMd5K[64] = {
    0xd76aa478, 0xe8c7b756, 0x242070db, 0xc1bdceee, 0xf57c0faf, 0x4787c62a, 0xa8304613, 0xfd469501,
    0x698098d8, 0x8b44f7af, 0xffff5bb1, 0x895cd7be, 0x6b901122, 0xfd987193, 0xa679438e, 0x49b40821,
    0xf61e2562, 0xc040b340, 0x265e5a51, 0xe9b6c7aa, 0xd62f105d, 0x02441453, 0xd8a1e681, 0xe7d3fbc8,
    0x21e1cde6, 0xc33707d6, 0xf4d50d87, 0x455a14ed, 0xa9e3e905, 0xfcefa3f8, 0x676f02d9, 0x8d2a4c8a,
    0xfffa3942, 0x8771f681, 0x6d9d6122, 0xfde5380c, 0xa4beea44, 0x4bdecfa9, 0xf6bb4b60, 0xbebfbc70,
    0x289b7ec6, 0xeaa127fa, 0xd4ef3085, 0x04881d05, 0xd9d4d039, 0xe6db99e5, 0x1fa27cf8, 0xc4ac5665,
    0xf4292244, 0x432aff97, 0xab9423a7, 0xfc93a039, 0x655b59c3, 0x8f0ccc92, 0xffeff47d, 0x85845dd1,
    0x6fa87e4f, 0xfe2ce6e0, 0xa3014314, 0x4e0811a1, 0xf7537e82, 0xbd3af235, 0x2ad7d2bb, 0xeb86d391,
};
constexpr int kMd5S[64] = {7, 12, 17, 22, 7, 12, 17, 22, 7, 12, 17, 22, 7, 12, 17, 22,
                           5, 9,  14, 20, 5, 9,  14, 20, 5, 9,  14, 20, 5, 9,  14, 20,
                           4, 11, 16, 23, 4, 11, 16, 23, 4, 11, 16, 23, 4, 11, 16, 23,
                           6, 10, 15, 21, 6, 10, 15, 21, 6, 10, 15, 21, 6, 10, 15, 21};

void md5_block(std::uint32_t state[4], const std::uint8_t* block) noexcept {
    std::uint32_t m[16];
    std::memcpy(m, block, 64);
    std::uint32_t a = state[0], b = state[1], c = state[2], d = state[3];
    for (int i = 0; i < 64; ++i) {
        std::uint32_t f;
        int g;
        if (i < 16)      { f = (b & c) | (~b & d); g = i; }
        else if (i < 32) { f = (d & b) | (~d & c); g = (5 * i + 1) % 16; }
        else if (i < 48) { f = b ^ c ^ d;          g = (3 * i + 5) % 16; }
        else             { f = c ^ (b | ~d);       g = (7 * i) % 16; }
        f += a + kMd5K[i] + m[g];
        a = d;
        d = c;
        c = b;
        b += std::rotl(f, kMd5S[i]);
    }
    state[0] += a;
    state[1] += b;
    state[2] += c;
    state[3] += d;
}

// ---- Shader tokens -------------------------------------------------------------

constexpr std::uint32_t kOpMad = 0x32, kOpMul = 0x38, kOpNop = 0x3A, kOpDclConstantBuffer = 0x59;
constexpr std::uint32_t kTypeTemp = 0, kTypeInput = 1, kTypeImmediate32 = 4, kTypeConstantBuffer = 8;
constexpr std::uint32_t kPoint001 = 0x3A83126F;   // 0.001f
constexpr std::uint32_t kSourceResolution = 39;   // cb2[39] = (W, H, 1/W, 1/H)

std::uint32_t opcode(std::uint32_t tok) { return tok & 0x7FF; }
std::uint32_t inst_length(std::uint32_t tok) { return (tok >> 24) & 0x7F; }
std::uint32_t op_type(std::uint32_t tok) { return (tok >> 12) & 0xFF; }
std::uint32_t index_dim(std::uint32_t tok) { return (tok >> 20) & 0x3; }
bool op_extended(std::uint32_t tok) { return tok >> 31; }

// One operand: its token position and the register index (1D operands).
struct Operand {
    std::size_t at;      // operand token
    std::size_t size;    // tokens including extensions, indices, immediates
    std::uint32_t type;
    std::uint32_t reg;   // index 0 for 1D operands
};

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

// DXBC checksum (MD5 compression with DXBC's own final block) of a blob;
// it is stored at bytes 4..19 of the container.
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

// ---- The dither pair -------------------------------------------------------------

std::uint32_t mask_of(std::uint32_t dst_tok) { return (dst_tok >> 4) & 0xF; }
bool mask_mode(std::uint32_t tok) { return (tok & 0x3) == 2 && ((tok >> 2) & 0x3) == 0; }
bool swizzle_mode(std::uint32_t tok) { return (tok & 0x3) == 2 && ((tok >> 2) & 0x3) == 1; }
std::uint32_t selector(std::uint32_t tok, int c) { return (tok >> (4 + 2 * c)) & 0x3; }

// A matched site: the `mul` and the `mad` right after it.
struct Site {
    std::size_t mul;        // token index of the mul's opcode
    std::size_t mad;        // token index of the mad's opcode
    std::size_t mad_src0;   // the mad's vPos operand
    std::uint32_t temp;     // rA
};

// The `mul rA.xy, vPos.xy.., l(0.001, 0.001, 0, 0)` at t[i], or nothing.
std::optional<Site> match_mul(const std::uint32_t* t, std::size_t i, std::size_t len) {
    const auto dst = parse_operand(t, i + len, i + 1);
    const auto a = dst ? parse_operand(t, i + len, dst->at + dst->size) : std::nullopt;
    const auto b = a ? parse_operand(t, i + len, a->at + a->size) : std::nullopt;
    if (!dst || !a || !b || b->at + b->size != i + len) return std::nullopt;
    if (dst->type != kTypeTemp || dst->size != 2 || !mask_mode(t[dst->at])) return std::nullopt;
    if ((mask_of(t[dst->at]) & 0x3) != 0x3) return std::nullopt;          // writes rA.x and rA.y
    if (a->type != kTypeInput || a->size != 2 || !swizzle_mode(t[a->at])) return std::nullopt;
    if (selector(t[a->at], 0) != 0 || selector(t[a->at], 1) != 1) return std::nullopt;  // vPos.xy..
    if (!is_immediate4(t, *b, kPoint001, kPoint001, 0, 0)) return std::nullopt;
    return Site{i, 0, 0, dst->reg};
}

// Completes `s` if the instruction at t[i] is its
// `mad rB, vPos.xxxy, l(0, 0, 0.001, 0.001), -vT.xxxx`.
bool match_mad(const std::uint32_t* t, std::size_t i, std::size_t len, Site& s) {
    const auto dst = parse_operand(t, i + len, i + 1);
    const auto a = dst ? parse_operand(t, i + len, dst->at + dst->size) : std::nullopt;
    const auto b = a ? parse_operand(t, i + len, a->at + a->size) : std::nullopt;
    const auto c = b ? parse_operand(t, i + len, b->at + b->size) : std::nullopt;
    if (!dst || !a || !b || !c || c->at + c->size != i + len) return false;
    const std::size_t mul_vpos = s.mul + 3;
    if (a->type != kTypeInput || a->size != 2 || a->reg != t[mul_vpos + 1]) return false;
    if (!swizzle_mode(t[a->at])) return false;
    for (int k = 0; k < 4; ++k)
        if (selector(t[a->at], k) > 1) return false;                       // reads vPos.x/.y only
    if (!is_immediate4(t, *b, 0, 0, kPoint001, kPoint001) || c->type != kTypeInput) return false;
    // The 0.001 scale of rA goes after the mad, so the mad must not write
    // any part of rA the mul wrote.
    if (dst->type == kTypeTemp && dst->reg == s.temp &&
        (!mask_mode(t[dst->at]) || (mask_of(t[dst->at]) & mask_of(t[s.mul + 1])) != 0))
        return false;
    s.mad = i;
    s.mad_src0 = a->at;
    return true;
}

} // namespace

std::vector<std::uint8_t> patch_position_dither(const void* src, std::size_t size, int& sites) noexcept {
    sites = 0;
    try {
        const auto* in = static_cast<const std::uint8_t*>(src);
        if (size < 32 || std::memcmp(in, "DXBC", 4) != 0) return {};

        // Cheap pre-filter: every site multiplies by the immediate
        // l(0.001, 0.001), two 0x3A83126F dwords in a row. Most shaders have
        // none, and skipping them avoids an MD5 of each (about 20 us a shader
        // on the game thread, half the cost of creating it).
        static constexpr std::uint8_t kTwoThousandths[8] = {0x6F, 0x12, 0x83, 0x3A, 0x6F, 0x12, 0x83, 0x3A};
        if (std::search(in, in + size, std::begin(kTwoThousandths), std::end(kTwoThousandths)) == in + size)
            return {};

        // The stored checksum must match ours, or we do not understand the blob.
        std::uint32_t sum[4];
        dxbc_checksum(in, size, sum);
        if (std::memcmp(sum, in + 4, 16) != 0) return {};

        std::uint32_t declared = 0, chunk_count = 0;
        std::memcpy(&declared, in + 24, 4);
        std::memcpy(&chunk_count, in + 28, 4);
        if (declared != size || 32 + std::size_t(chunk_count) * 4 > size) return {};
        std::size_t shex_chunk = 0, shex_bytes = 0;
        for (std::uint32_t c = 0; c < chunk_count; ++c) {
            std::uint32_t off = 0;
            std::memcpy(&off, in + 32 + 4 * std::size_t(c), 4);
            if (std::size_t(off) + 8 > size) return {};
            if (std::memcmp(in + off, "SHEX", 4) == 0 || std::memcmp(in + off, "SHDR", 4) == 0) {
                if (shex_chunk) return {};
                std::uint32_t bytes = 0;
                std::memcpy(&bytes, in + off + 4, 4);
                if (std::size_t(off) + 8 + bytes > size || bytes % 4) return {};
                shex_chunk = off;
                shex_bytes = bytes;
            }
        }
        if (!shex_chunk || shex_bytes < 8) return {};

        // The program, as tokens.
        std::vector<std::uint32_t> t(shex_bytes / 4);
        std::memcpy(t.data(), in + shex_chunk + 8, shex_bytes);
        if ((t[0] >> 16 & 0xFFFF) != 0x0000) return {};  // pixel shader program type is 0
        if (t[1] < 2 || t[1] > t.size()) return {};
        const std::size_t end = t[1];

        // Pass 1: find the pairs and the cb2 declaration.
        std::size_t cb2_decl = 0;
        std::vector<Site> found;
        std::optional<Site> pending;     // a matched `mul` waiting for its `mad`
        for (std::size_t i = 2; i < end;) {
            const std::uint32_t tok = t[i];
            const std::uint32_t op = opcode(tok);
            std::size_t len = inst_length(tok);
            if (op == 0x35 /* customdata */) {
                if (i + 1 >= end) return {};
                len = t[i + 1];
            }
            if (len == 0 || i + len > end) return {};

            std::optional<Site> next;
            if (op == kOpDclConstantBuffer && len == 4 && op_type(t[i + 1]) == kTypeConstantBuffer &&
                t[i + 2] == 2) {
                cb2_decl = i;
            } else if (op == kOpMul && !(tok >> 31) && len == 10) {
                next = match_mul(t.data(), i, len);
            } else if (op == kOpMad && pending && !(tok >> 31)) {
                if (match_mad(t.data(), i, len, *pending)) found.push_back(*pending);
            }
            pending = next;                // the pair must be adjacent
            i += len;
        }
        if (found.empty() || !cb2_decl) return {};   // no site, or cb2 would need a declaration

        // Pass 2: rewrite. Per site the 10-token mul becomes an 8-token one
        // and a 10-token scale follows the mad (8 tokens more per site).
        if (t[cb2_decl + 3] < kSourceResolution + 1) t[cb2_decl + 3] = kSourceResolution + 1;
        std::vector<std::uint32_t> prog;
        prog.reserve(t.size() + found.size() * 8);
        const auto copy = [&](std::size_t from, std::size_t to) {
            prog.insert(prog.end(), t.begin() + static_cast<std::ptrdiff_t>(from),
                        t.begin() + static_cast<std::ptrdiff_t>(to));
        };
        std::size_t from = 0;
        for (const Site& s : found) {
            copy(from, s.mul);
            // mul rA, vPos, cb2[39].zwzz: src1 is 3 tokens in place of 5.
            prog.push_back((t[s.mul] & ~(0x7Fu << 24)) | (8u << 24));
            copy(s.mul + 1, s.mul + 5);
            const std::uint32_t swizzle_zwzz = (2u << 0) | (3u << 2) | (2u << 4) | (2u << 6);
            // 4 components, swizzle mode, type cb, 2D, immediate indices.
            prog.push_back(2u | (1u << 2) | (swizzle_zwzz << 4) | (kTypeConstantBuffer << 12) | (2u << 20));
            prog.push_back(2);
            prog.push_back(kSourceResolution);

            // mad: src0 vPos -> rA, same swizzle.
            const std::size_t mad_len = inst_length(t[s.mad]);
            const std::size_t src0 = prog.size() + (s.mad_src0 - s.mad);
            copy(s.mad, s.mad + mad_len);
            prog[src0] = (prog[src0] & ~(0xFFu << 12)) | (kTypeTemp << 12);
            prog[src0 + 1] = s.temp;

            // mul rA.<mask>, rA.xyzw, l(0.001, 0.001, 0, 0): the mul's own
            // opcode, destination and immediate, with rA as the source.
            const std::uint32_t swizzle_xyzw = (0u << 0) | (1u << 2) | (2u << 4) | (3u << 6);
            copy(s.mul, s.mul + 3);
            prog.push_back(2u | (1u << 2) | (swizzle_xyzw << 4) | (kTypeTemp << 12) | (1u << 20));
            prog.push_back(s.temp);
            copy(s.mul + 5, s.mul + 10);

            from = s.mad + mad_len;
        }
        copy(from, t.size());
        const std::size_t grow = (prog.size() - t.size()) * 4;
        prog[1] = static_cast<std::uint32_t>(t[1] + grow / 4);

        // Reassemble the container: the bytes up to the program, the program,
        // the chunks after it, whose offsets move by `grow`.
        const std::size_t shex_end = shex_chunk + 8 + shex_bytes;
        std::vector<std::uint8_t> out;
        out.reserve(size + grow);
        out.insert(out.end(), in, in + shex_chunk + 8);
        const auto* p = reinterpret_cast<const std::uint8_t*>(prog.data());
        out.insert(out.end(), p, p + prog.size() * 4);
        out.insert(out.end(), in + shex_end, in + size);

        const auto put = [&out](std::size_t at, std::size_t v) {
            const auto u = static_cast<std::uint32_t>(v);
            std::memcpy(out.data() + at, &u, 4);
        };
        put(24, out.size());
        put(shex_chunk + 4, shex_bytes + grow);
        for (std::uint32_t c = 0; c < chunk_count; ++c) {
            std::uint32_t off = 0;
            std::memcpy(&off, in + 32 + 4 * std::size_t(c), 4);
            if (off > shex_chunk) put(32 + 4 * std::size_t(c), std::size_t(off) + grow);
        }

        dxbc_checksum(out.data(), out.size(), sum);
        std::memcpy(out.data() + 4, sum, 16);
        sites = static_cast<int>(found.size());
        return out;
    } catch (...) {
        sites = 0;
        return {};
    }
}

} // namespace mothervr::render
