#include "core/render/dither_fix.h"

#include "core/render/dxbc.h"
#include "core/render/dxbc_tokens.h"
#include "core/render/tuning.h"

#include <algorithm>
#include <cstring>
#include <iterator>
#include <optional>

namespace omvr::core::render {

namespace {

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

} // namespace omvr::core::render
