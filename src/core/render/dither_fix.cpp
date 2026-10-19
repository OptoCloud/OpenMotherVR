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

// ---- Operands --------------------------------------------------------------------

// Tokens of a register operand with one immediate index, of a negated one
// (the modifier token between them) and of a cb operand (two indices).
constexpr std::size_t kRegisterOperandSize = 2;
constexpr std::size_t kNegatedOperandSize  = 3;
constexpr std::size_t kComponents          = 4;
// dcl_constantBuffer: opcode, operand token, buffer index, register count.
constexpr std::size_t kDclCbLength   = 4;
constexpr std::size_t kDclCbIndex    = 2;
constexpr std::size_t kDclCbRegCount = 3;
// Instructions the rewrite emits: opcode plus operands, in tokens.
constexpr std::uint32_t kImmediateOperandSize = 5;
constexpr std::uint32_t kCbOperandSize        = 3;
constexpr auto kMulImmediateLength =
    static_cast<std::uint32_t>(1 + 2 * kRegisterOperandSize + kImmediateOperandSize);
constexpr auto kMulCbLength = static_cast<std::uint32_t>(1 + 2 * kRegisterOperandSize + kCbOperandSize);
constexpr auto kFrcLength   = static_cast<std::uint32_t>(1 + 2 * kRegisterOperandSize);

// 1/1024 as an immediate: the wrapped position's scale (08 §1.4 r7).
constexpr std::uint32_t kWrapScale = std::bit_cast<std::uint32_t>(1.0f / kDitherWrapPixels);

std::uint32_t mask_of(std::uint32_t tok) { return (tok >> kComponentShift) & kWriteMaskBits; }
bool mask_mode(std::uint32_t tok) {
    return (tok & kComponentCountMask) == kFourComponents && selection_mode(tok) == kSelectMask;
}
bool swizzle_mode(std::uint32_t tok) {
    return (tok & kComponentCountMask) == kFourComponents && selection_mode(tok) == kSelectSwizzle;
}
std::uint32_t selector(std::uint32_t tok, std::uint32_t c) {
    return (tok >> (kComponentShift + kSwizzleBits * c)) & kSwizzleMask;
}
// The opcode token is `op` with no flags (no saturate, no extension).
bool plain(std::uint32_t tok, std::uint32_t op) { return (tok & ~(kLengthMask << kLengthShift)) == op; }

// The two components of a write mask with exactly two bits, in order.
struct Pair {
    std::uint32_t first  = 0;
    std::uint32_t second = 0;

    std::uint32_t mask() const { return (1u << first) | (1u << second); }
};

std::optional<Pair> pair_of(std::uint32_t mask) {
    if (std::popcount(mask) != 2) return std::nullopt;
    const auto first  = static_cast<std::uint32_t>(std::countr_zero(mask));
    const auto second = static_cast<std::uint32_t>(std::countr_zero(mask & (mask - 1)));
    return Pair{first, second};
}

// A swizzle with `a` in component p.first, `b` in p.second and `a` elsewhere.
std::uint32_t swizzle_into(Pair p, std::uint32_t a, std::uint32_t b) {
    std::array<std::uint32_t, kComponents> s{a, a, a, a};
    s[p.second] = b;
    return swizzle(s[kX], s[kY], s[kZ], s[kW]);
}

// The 4-component immediate with `value` in the components of `p`, 0 elsewhere.
std::array<std::uint32_t, kComponents> immediate_in(Pair p, std::uint32_t value) {
    std::array<std::uint32_t, kComponents> v{};
    v[p.first]  = value;
    v[p.second] = value;
    return v;
}

// ---- The dither sequence (08 §1.1) ----------------------------------------------

// The N operands of the instruction at t[i], which must end at its end.
template <std::size_t N>
std::optional<std::array<Operand, N>> operands(const std::uint32_t* t, std::size_t i, std::size_t len) {
    std::array<Operand, N> ops{};
    std::size_t at = i + 1;
    for (Operand& o : ops) {
        const auto parsed = parse_operand(t, i + len, at);
        if (!parsed) return std::nullopt;
        o  = *parsed;
        at = o.at + o.size;
    }
    if (at != i + len) return std::nullopt;
    return ops;
}

// One written pair: the temporary and its two components.
struct Written {
    std::uint32_t reg = 0;
    Pair comps;
};

// A temporary written in exactly two components, without modifiers.
std::optional<Written> two_component_temp(const std::uint32_t* t, const Operand& o) {
    if (o.type != kTypeTemp || o.size != kRegisterOperandSize || !mask_mode(t[o.at])) return std::nullopt;
    const auto p = pair_of(mask_of(t[o.at]));
    if (!p) return std::nullopt;
    return Written{o.reg, *p};
}

// `vP` read with vP.x in component p.first and vP.y in p.second.
bool reads_position(const std::uint32_t* t, const Operand& o, Pair p) {
    return o.type == kTypeInput && o.size == kRegisterOperandSize && swizzle_mode(t[o.at]) &&
           selector(t[o.at], p.first) == kX && selector(t[o.at], p.second) == kY;
}

// The immediate holds 0.001 in exactly the components of `p`, 0 elsewhere.
bool thousandths_in(const std::uint32_t* t, const Operand& o, Pair p) {
    const auto v = immediate_in(p, kPoint001);
    return is_immediate4(t, o, v[kX], v[kY], v[kZ], v[kW]);
}

// A source reading the written pair `w` in its first two components.
bool reads_pair(const std::uint32_t* t, const Operand& o, const Written& w) {
    return o.type == kTypeTemp && o.size == kRegisterOperandSize && swizzle_mode(t[o.at]) && o.reg == w.reg &&
           selector(t[o.at], kX) == w.comps.first && selector(t[o.at], kY) == w.comps.second;
}

// A matched site: instruction 1 (`mul`), instruction 2 (`mad`) right after
// it, and the `dp2` right after that reading both.
struct Site {
    std::size_t mul  = 0; // token index of the mul's opcode
    std::size_t mad  = 0; // token index of the mad's opcode
    std::size_t dp2  = 0; // token index of the dp2's opcode
    std::size_t vpos = 0; // the mad's vP operand
    Written a;            // rA, what the mul writes
    Written b;            // rB, what the mad writes
};

// Instruction 1: `mul rA.<p>, vP.<x, y in p>, l(0.001 in p)`.
bool match_mul(const std::uint32_t* t, std::size_t i, std::size_t len, Site& s, std::uint32_t& position) {
    if (!plain(t[i], kOpMul)) return false;
    const auto ops = operands<3>(t, i, len);
    if (!ops) return false;
    const auto& [dst, vp, imm] = *ops;
    const auto a               = two_component_temp(t, dst);
    if (!a || !reads_position(t, vp, a->comps) || !thousandths_in(t, imm, a->comps)) return false;
    s.mul    = i;
    s.a      = *a;
    position = vp.reg;
    return true;
}

// Instruction 2: `mad rB.<p>, vP.<x, y in p>, l(0.001 in p), -vT.<one
// component in p>`, the same vP, and nothing instruction 1 wrote.
bool match_mad(const std::uint32_t* t, std::size_t i, std::size_t len, Site& s, std::uint32_t position) {
    if (!plain(t[i], kOpMad)) return false;
    const auto ops = operands<4>(t, i, len);
    if (!ops) return false;
    const auto& [dst, vp, imm, tex] = *ops;
    const auto b                    = two_component_temp(t, dst);
    if (!b || vp.reg != position || !reads_position(t, vp, b->comps) || !thousandths_in(t, imm, b->comps))
        return false;
    if (tex.type != kTypeInput || tex.size != kNegatedOperandSize || t[tex.at + 1] != kNegateModifier ||
        !swizzle_mode(t[tex.at]) ||
        selector(t[tex.at], b->comps.first) != selector(t[tex.at], b->comps.second))
        return false;
    if (b->reg == s.a.reg && (b->comps.mask() & s.a.comps.mask()) != 0) return false;
    s.mad  = i;
    s.vpos = vp.at;
    s.b    = *b;
    return true;
}

// Instruction 3: `dp2` of the two written pairs, in written order, either
// source first.
bool match_dp2(const std::uint32_t* t, std::size_t i, std::size_t len, Site& s) {
    if (!plain(t[i], kOpDp2)) return false;
    const auto ops = operands<3>(t, i, len);
    if (!ops) return false;
    const auto& [dst, x, y] = *ops;
    if (dst.type != kTypeTemp || dst.size != kRegisterOperandSize || !mask_mode(t[dst.at])) return false;
    if (!(reads_pair(t, x, s.a) && reads_pair(t, y, s.b)) &&
        !(reads_pair(t, x, s.b) && reads_pair(t, y, s.a)))
        return false;
    s.dp2 = i;
    return true;
}

// ---- The program -----------------------------------------------------------------

// The program's instructions (token index of each opcode), or nothing when
// one does not fit.
std::optional<std::vector<std::size_t>> instructions(const std::vector<std::uint32_t>& t, std::size_t end) {
    std::vector<std::size_t> at;
    for (std::size_t i = kProgramHeaderTokens; i < end;) {
        std::size_t len = inst_length(t[i]);
        if (opcode(t[i]) == kOpCustomData) {
            if (i + 1 >= end) return std::nullopt;
            len = t[i + 1];
        }
        // Not `i + len > end`: a customData length near 2^32 wraps
        // size_t on Win32 and would walk backwards forever.
        if (len == 0 || len > end - i) return std::nullopt;
        at.push_back(i);
        i += len;
    }
    return at;
}

std::size_t length_at(const std::vector<std::uint32_t>& t, std::size_t i) {
    return opcode(t[i]) == kOpCustomData ? t[i + 1] : inst_length(t[i]);
}

bool is_cb2_declaration(const std::vector<std::uint32_t>& t, std::size_t i) {
    return opcode(t[i]) == kOpDclConstantBuffer && length_at(t, i) == kDclCbLength &&
           op_type(t[i + 1]) == kTypeConstantBuffer && t[i + kDclCbIndex] == kResolutionBuffer;
}

// The program's dither sites and its cb2 declaration (token index), or
// nothing when it has no site or anything unexpected.
struct Sites {
    std::vector<Site> found;
    std::optional<std::size_t> cb2_decl;
};

std::optional<Sites> find_sites(const std::vector<std::uint32_t>& t, std::size_t end) {
    const auto at = instructions(t, end);
    if (!at) return std::nullopt;
    Sites s;
    const std::size_t n = at->size();
    for (std::size_t k = 0; k < n; ++k) {
        const std::size_t i = (*at)[k];
        if (is_cb2_declaration(t, i)) s.cb2_decl = i;
        if (k + 2 >= n) continue;
        const std::size_t j = (*at)[k + 1], l = (*at)[k + 2];
        Site site;
        std::uint32_t position = 0;
        if (match_mul(t.data(), i, length_at(t, i), site, position) &&
            match_mad(t.data(), j, length_at(t, j), site, position) &&
            match_dp2(t.data(), l, length_at(t, l), site))
            s.found.push_back(site);
    }
    if (s.found.empty()) return std::nullopt;
    return s;
}

// Builds the rewritten program token by token.
class Rewriter {
public:
    explicit Rewriter(std::vector<std::uint32_t>& prog) : prog_(prog) {}

    void copy(std::span<const std::uint32_t> tokens) {
        prog_.insert(prog_.end(), tokens.begin(), tokens.end());
    }
    void push(std::uint32_t tok) { prog_.push_back(tok); }
    std::size_t size() const { return prog_.size(); }
    std::uint32_t& operator[](std::size_t i) { return prog_[i]; }

    // `op rA.<pair>, ...`: opcode and destination.
    void begin(std::uint32_t op, std::uint32_t length, const Written& a) {
        push(opcode_token(op, length));
        push(masked_destination(a.comps.mask(), kTypeTemp, kIndex1D));
        push(a.reg);
    }
    // rA.xyzw as a source.
    void temp(const Written& a) {
        push(swizzled_source(swizzle(kX, kY, kZ, kW), kTypeTemp, kIndex1D));
        push(a.reg);
    }
    void immediate(Pair p, std::uint32_t value) {
        push(kFourComponents | (kTypeImmediate32 << kTypeShift));
        for (const std::uint32_t v : immediate_in(p, value)) push(v);
    }

private:
    std::vector<std::uint32_t>& prog_;
};

// The site rewritten (see dither_fix.h) into `w`: rA = n, rB = 0.001 n - t,
// rA = 0.001 n.
void rewrite_site(const std::vector<std::uint32_t>& t, const Site& s, bool has_cb2, Rewriter& w) {
    const std::span<const std::uint32_t> all{t};
    const std::size_t vpos_at = s.mul + 1 + kRegisterOperandSize;
    if (has_cb2) {
        // mul rA, vP, cb2[39] with 1/W, 1/H (zw) in rA's components.
        w.begin(kOpMul, kMulCbLength, s.a);
        w.copy(all.subspan(vpos_at, kRegisterOperandSize));
        w.push(swizzled_source(swizzle_into(s.a.comps, kZ, kW), kTypeConstantBuffer, kIndex2D));
        w.push(kResolutionBuffer);
        w.push(kSourceResolution);
    } else {
        // mul rA, vP, l(1/1024); frc rA, rA.
        w.begin(kOpMul, kMulImmediateLength, s.a);
        w.copy(all.subspan(vpos_at, kRegisterOperandSize));
        w.immediate(s.a.comps, kWrapScale);
        w.begin(kOpFrc, kFrcLength, s.a);
        w.temp(s.a);
    }
    // The mad, reading rA's pair where it read vP.x and vP.y.
    const std::size_t mad_at = w.size();
    w.copy(all.subspan(s.mad, s.dp2 - s.mad));
    const std::size_t src = mad_at + (s.vpos - s.mad);
    w[src] = swizzled_source(swizzle_into(s.b.comps, s.a.comps.first, s.a.comps.second), kTypeTemp, kIndex1D);
    w[src + 1] = s.a.reg;
    // mul rA, rA, l(0.001).
    w.begin(kOpMul, kMulImmediateLength, s.a);
    w.temp(s.a);
    w.immediate(s.a.comps, kPoint001);
}

std::vector<std::uint32_t> rewrite(std::vector<std::uint32_t>& t, const Sites& sites) {
    if (sites.cb2_decl) {
        // cb2 must declare the resolution register.
        std::uint32_t& registers = t[*sites.cb2_decl + kDclCbRegCount];
        registers                = std::max(registers, kSourceResolution + 1);
    }
    std::vector<std::uint32_t> prog;
    prog.reserve(t.size() + sites.found.size() * (kMulImmediateLength + kFrcLength));
    Rewriter w{prog};
    const std::span<const std::uint32_t> all{t};
    std::size_t from = 0;
    for (const Site& s : sites.found) {
        w.copy(all.subspan(from, s.mul - from));
        rewrite_site(t, s, sites.cb2_decl.has_value(), w);
        from = s.dp2;
    }
    w.copy(all.subspan(from));
    prog[1] = static_cast<std::uint32_t>(t[1] + (prog.size() - t.size()));
    return prog;
}

// The pre-filter: the immediate l(0.001, 0.001) of every site, two 0.001f
// words in a row as they lie in the blob (08 §1.4).
consteval std::array<std::uint8_t, 2 * kTokenBytes> two_thousandths() {
    const auto word = std::bit_cast<std::array<std::uint8_t, kTokenBytes>>(kPoint001);
    std::array<std::uint8_t, 2 * kTokenBytes> bytes{};
    std::ranges::copy(word, bytes.begin());
    std::ranges::copy(word, bytes.begin() + kTokenBytes);
    return bytes;
}
constexpr auto kTwoThousandths = two_thousandths();

void patch(std::span<const std::uint8_t> in, DitherPatch& result) {
    if (in.size() < kContainerHeaderSize || std::memcmp(in.data(), kDxbcMagic.data(), kDxbcMagicBytes) != 0)
        return;
    // Cheap pre-filter: most shaders have no site, and skipping them avoids
    // an MD5 of each (about 20 us a shader on the game thread, half the cost
    // of creating it).
    if (std::ranges::search(in, kTwoThousandths).empty()) return;
    result.has_constants = true;

    const auto chunk = find_program(in);
    if (!chunk) return;
    std::vector<std::uint32_t> t = program_tokens(in, *chunk);
    if (((t[0] >> kProgramTypeShift) & kProgramTypeMask) != kPixelShaderProgram) return;
    if (t[1] < kProgramHeaderTokens || t[1] > t.size()) return;

    const auto found = find_sites(t, t[1]);
    if (!found) return;
    const auto prog = rewrite(t, *found);
    result.blob     = reassemble(in, *chunk, prog);
    result.sites    = static_cast<int>(found->found.size());
}

} // namespace

DitherPatch patch_position_dither(const void* src, std::size_t size) noexcept {
    DitherPatch result;
    try {
        patch({static_cast<const std::uint8_t*>(src), size}, result);
    } catch (...) {
        // An allocation failed: the original shader is used.
        result.blob.clear();
        result.sites = 0;
    }
    return result;
}

} // namespace omvr::core::render
