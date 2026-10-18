// OpenMotherVR-Provenance: clean
#include "core/render/dither_fix.h"

#include "core/render/dxbc.h"
#include "core/render/dxbc_tokens.h"
#include "core/render/tuning.h"

#include <algorithm>
#include <array>
#include <bit>
#include <cstring>
#include <optional>
#include <span>

namespace omvr::core::render {

namespace {

// ---- The dither pair -------------------------------------------------------------

// The mul's tokens: opcode, destination (token, register), source 0 (token,
// register), then the 5-token immediate. Rewritten, source 1 is a 3-token
// constant-buffer operand.
constexpr std::size_t kMulImmediateLength = 10;
constexpr std::size_t kMulSrc0            = 3;
constexpr std::size_t kMulImmediate       = 5;
constexpr std::uint32_t kMulCbLength      = 8;
// dcl_constantBuffer: opcode, operand token, buffer index, register count.
constexpr std::size_t kDclCbLength   = 4;
constexpr std::size_t kDclCbIndex    = 2;
constexpr std::size_t kDclCbRegCount = 3;
// Each site's program grows by the inserted 10-token mul less the 2 tokens
// the first mul loses: 8 tokens, 32 bytes.
constexpr std::size_t kMulShrinkTokens  = kMulImmediateLength - kMulCbLength;
constexpr std::size_t kSiteGrowthTokens = kMulImmediateLength - kMulShrinkTokens;

std::uint32_t mask_of(std::uint32_t dst_tok) { return (dst_tok >> kComponentShift) & kWriteMaskBits; }
bool mask_mode(std::uint32_t tok) {
    return (tok & kComponentCountMask) == kFourComponents && selection_mode(tok) == kSelectMask;
}
bool swizzle_mode(std::uint32_t tok) {
    return (tok & kComponentCountMask) == kFourComponents && selection_mode(tok) == kSelectSwizzle;
}
std::uint32_t selector(std::uint32_t tok, std::uint32_t c) {
    return (tok >> (kComponentShift + kSwizzleBits * c)) & kSwizzleMask;
}

// The pre-filter: the immediate l(0.001, 0.001) every site multiplies by,
// two 0.001f words in a row as they lie in the blob.
consteval std::array<std::uint8_t, 2 * kTokenBytes> two_thousandths() {
    const auto word = std::bit_cast<std::array<std::uint8_t, kTokenBytes>>(kPoint001);
    std::array<std::uint8_t, 2 * kTokenBytes> bytes{};
    std::ranges::copy(word, bytes.begin());
    std::ranges::copy(word, bytes.begin() + kTokenBytes);
    return bytes;
}
constexpr auto kTwoThousandths = two_thousandths();

// A matched site: the `mul` and the `mad` right after it.
struct Site {
    std::size_t mul;      // token index of the mul's opcode
    std::size_t mad;      // token index of the mad's opcode
    std::size_t mad_src0; // the mad's vPos operand
    std::uint32_t temp;   // rA
};

// The `mul rA.xy, vPos.xy.., l(0.001, 0.001, 0, 0)` at t[i], or nothing.
std::optional<Site> match_mul(const std::uint32_t* t, std::size_t i, std::size_t len) {
    const auto dst = parse_operand(t, i + len, i + 1);
    const auto a   = dst ? parse_operand(t, i + len, dst->at + dst->size) : std::nullopt;
    const auto b   = a ? parse_operand(t, i + len, a->at + a->size) : std::nullopt;
    if (!dst || !a || !b || b->at + b->size != i + len) return std::nullopt;
    if (dst->type != kTypeTemp || dst->size != 2 || !mask_mode(t[dst->at])) return std::nullopt;
    if ((mask_of(t[dst->at]) & kMaskXY) != kMaskXY) return std::nullopt; // writes rA.x and rA.y
    if (a->type != kTypeInput || a->size != 2 || !swizzle_mode(t[a->at])) return std::nullopt;
    if (selector(t[a->at], 0) != kX || selector(t[a->at], 1) != kY) return std::nullopt; // vPos.xy..
    if (!is_immediate4(t, *b, kPoint001, kPoint001, 0, 0)) return std::nullopt;
    return Site{i, 0, 0, dst->reg};
}

// Completes `s` if the instruction at t[i] is its
// `mad rB, vPos.xxxy, l(0, 0, 0.001, 0.001), -vT.xxxx`.
bool match_mad(const std::uint32_t* t, std::size_t i, std::size_t len, Site& s) {
    const auto dst = parse_operand(t, i + len, i + 1);
    const auto a   = dst ? parse_operand(t, i + len, dst->at + dst->size) : std::nullopt;
    const auto b   = a ? parse_operand(t, i + len, a->at + a->size) : std::nullopt;
    const auto c   = b ? parse_operand(t, i + len, b->at + b->size) : std::nullopt;
    if (!dst || !a || !b || !c || c->at + c->size != i + len) return false;
    const std::size_t mul_vpos = s.mul + kMulSrc0;
    if (a->type != kTypeInput || a->size != 2 || a->reg != t[mul_vpos + 1]) return false;
    if (!swizzle_mode(t[a->at])) return false;
    for (std::uint32_t k = kX; k <= kW; ++k) {
        if (selector(t[a->at], k) > kY) return false; // reads vPos.x/.y only
    }
    if (!is_immediate4(t, *b, 0, 0, kPoint001, kPoint001) || c->type != kTypeInput) return false;
    // The 0.001 scale of rA goes after the mad, so the mad must not write
    // any part of rA the mul wrote.
    if (dst->type == kTypeTemp && dst->reg == s.temp &&
        (!mask_mode(t[dst->at]) || (mask_of(t[dst->at]) & mask_of(t[s.mul + 1])) != 0))
        return false;
    s.mad      = i;
    s.mad_src0 = a->at;
    return true;
}

// ---- The program -----------------------------------------------------------------

// The program's dither sites and its cb2 declaration (token index), or
// nothing when it has no site, no cb2 declaration, or anything unexpected.
struct Sites {
    std::vector<Site> found;
    std::size_t cb2_decl = 0;
};

bool is_cb2_declaration(const std::vector<std::uint32_t>& t, std::size_t i, std::uint32_t op,
                        std::size_t len) {
    return op == kOpDclConstantBuffer && len == kDclCbLength && op_type(t[i + 1]) == kTypeConstantBuffer &&
           t[i + kDclCbIndex] == kResolutionBuffer;
}

std::optional<Sites> find_sites(const std::vector<std::uint32_t>& t, std::size_t end) {
    Sites s;
    std::optional<Site> pending; // a matched `mul` waiting for its `mad`
    for (std::size_t i = kProgramHeaderTokens; i < end;) {
        const std::uint32_t tok = t[i];
        const std::uint32_t op  = opcode(tok);
        std::size_t len         = inst_length(tok);
        if (op == kOpCustomData) {
            if (i + 1 >= end) return std::nullopt;
            len = t[i + 1];
        }
        if (len == 0 || i + len > end) return std::nullopt;

        std::optional<Site> next;
        if (is_cb2_declaration(t, i, op, len)) {
            s.cb2_decl = i;
        } else if (op == kOpMul && !op_extended(tok) && len == kMulImmediateLength) {
            next = match_mul(t.data(), i, len);
        } else if (op == kOpMad && pending && !op_extended(tok)) {
            if (match_mad(t.data(), i, len, *pending)) s.found.push_back(*pending);
        }
        pending = next; // the pair must be adjacent
        i += len;
    }
    if (s.found.empty() || !s.cb2_decl) return std::nullopt; // no site, or cb2 would need a declaration
    return s;
}

// The program with every site rewritten (see dither_fix.h): per site the
// 10-token mul becomes an 8-token one and a 10-token scale follows the mad.
std::vector<std::uint32_t> rewrite(std::vector<std::uint32_t>& t, const Sites& sites) {
    // cb2 must declare the resolution register.
    std::uint32_t& registers = t[sites.cb2_decl + kDclCbRegCount];
    if (registers < kSourceResolution + 1) registers = kSourceResolution + 1;

    std::vector<std::uint32_t> prog;
    prog.reserve(t.size() + sites.found.size() * kSiteGrowthTokens);
    const auto copy = [&](std::size_t from, std::size_t to) {
        prog.insert(prog.end(), t.begin() + static_cast<std::ptrdiff_t>(from),
                    t.begin() + static_cast<std::ptrdiff_t>(to));
    };
    std::size_t from = 0;
    for (const Site& s : sites.found) {
        copy(from, s.mul);
        // mul rA, vPos, cb2[39].zwzz: src1 is 3 tokens in place of 5.
        prog.push_back(with_length(t[s.mul], kMulCbLength));
        copy(s.mul + 1, s.mul + kMulImmediate);
        prog.push_back(swizzled_source(swizzle(kZ, kW, kZ, kZ), kTypeConstantBuffer, kIndex2D));
        prog.push_back(kResolutionBuffer);
        prog.push_back(kSourceResolution);

        // mad: src0 vPos -> rA, same swizzle.
        const std::size_t mad_len = inst_length(t[s.mad]);
        const std::size_t src0    = prog.size() + (s.mad_src0 - s.mad);
        copy(s.mad, s.mad + mad_len);
        prog[src0]     = with_type(prog[src0], kTypeTemp);
        prog[src0 + 1] = s.temp;

        // mul rA.<mask>, rA.xyzw, l(0.001, 0.001, 0, 0): the mul's own
        // opcode, destination and immediate, with rA as the source.
        copy(s.mul, s.mul + kMulSrc0);
        prog.push_back(swizzled_source(swizzle(kX, kY, kZ, kW), kTypeTemp, kIndex1D));
        prog.push_back(s.temp);
        copy(s.mul + kMulImmediate, s.mul + kMulImmediateLength);

        from = s.mad + mad_len;
    }
    copy(from, t.size());
    prog[1] = static_cast<std::uint32_t>(t[1] + (prog.size() - t.size()));
    return prog;
}

std::vector<std::uint8_t> patch(std::span<const std::uint8_t> in, int& sites) {
    if (in.size() < kContainerHeaderSize || std::memcmp(in.data(), kDxbcMagic.data(), kDxbcMagicBytes) != 0)
        return {};
    // Cheap pre-filter: most shaders have no site, and skipping them avoids
    // an MD5 of each (about 20 us a shader on the game thread, half the cost
    // of creating it).
    if (std::ranges::search(in, kTwoThousandths).empty()) return {};

    const auto chunk = find_program(in);
    if (!chunk) return {};
    std::vector<std::uint32_t> t = program_tokens(in, *chunk);
    if (((t[0] >> kProgramTypeShift) & kProgramTypeMask) != kPixelShaderProgram) return {};
    if (t[1] < kProgramHeaderTokens || t[1] > t.size()) return {};

    const auto found = find_sites(t, t[1]);
    if (!found) return {};
    const auto prog = rewrite(t, *found);
    auto out        = reassemble(in, *chunk, prog);
    sites           = static_cast<int>(found->found.size());
    return out;
}

} // namespace

std::vector<std::uint8_t> patch_position_dither(const void* src, std::size_t size, int& sites) noexcept {
    sites = 0;
    try {
        return patch({static_cast<const std::uint8_t*>(src), size}, sites);
    } catch (...) {
        // An allocation failed: the original shader is used.
        sites = 0;
        return {};
    }
}

} // namespace omvr::core::render
