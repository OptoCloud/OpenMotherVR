// OpenMotherVR-Provenance: clean
#include "core/render/upright_sprites.h"

#include "core/render/dxbc.h"
#include "core/render/dxbc_tokens.h"
#include "core/render/tuning.h"

#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <cstring>
#include <initializer_list>
#include <optional>
#include <span>

namespace omvr::core::render {

// ---- The arithmetic (24 §14.3) ---------------------------------------------------

util::Vec2 sprite_up_in_view(float ux, float uy) noexcept {
    const float len = std::sqrt(ux * ux + uy * uy);
    util::Vec2 w{ux, uy + std::max(0.0f, kUprightEaseLength - len)};
    float len2 = w.x * w.x + w.y * w.y;
    if (!(len2 >= kUprightDegenerateLength2)) { // also NaN
        w    = {0.0f, 1.0f};
        len2 = 1.0f;
    }
    const float inv = 1.0f / std::sqrt(len2);
    return {w.x * inv, w.y * inv};
}

util::Vec2 upright_offset(util::Vec2 offset, util::Vec2 up) noexcept {
    return {offset.x * up.y + offset.y * up.x, offset.y * up.y - offset.x * up.x};
}

SpriteAxes upright_sprite_axes(const util::Vec3& right, const util::Vec3& up,
                               const util::Vec3& forward) noexcept {
    // The world's up (+Y) in view x and y: its dot products with the
    // camera's right and up axes.
    const util::Vec2 n = sprite_up_in_view(right.y, up.y);
    const auto mix     = [](const util::Vec3& p, float s, const util::Vec3& q, float t) {
        return util::Vec3{p.x * s + q.x * t, p.y * s + q.y * t, p.z * s + q.z * t};
    };
    // The view-plane axes (n.y, -n.x) and (n.x, n.y) in world space.
    return {mix(right, n.y, up, -n.x), mix(right, n.x, up, n.y), forward};
}

namespace {

// ---- Operands --------------------------------------------------------------------

std::uint32_t mask_of(std::uint32_t tok) { return (tok >> kComponentShift) & kWriteMaskBits; }
std::uint32_t selected_component(std::uint32_t tok) { return (tok >> kComponentShift) & kSwizzleMask; }
bool is_select1(std::uint32_t tok) {
    return (tok & kComponentCountMask) == kFourComponents && selection_mode(tok) == kSelect1;
}
bool is_masked(std::uint32_t tok) {
    return (tok & kComponentCountMask) == kFourComponents && selection_mode(tok) == kSelectMask;
}
std::uint32_t bit_of(std::uint32_t component) { return 1u << component; }

// One component of a temporary, as a source: `rN.c` or `-rN.c`. Its tokens
// as they lie in the program (2, or 3 with the negate).
struct TempComponent {
    std::size_t at          = 0;
    std::size_t size        = 0;
    std::uint32_t reg       = 0;
    std::uint32_t component = 0;
};

std::optional<TempComponent> temp_component(const std::uint32_t* t, const Operand& o, bool negate_allowed) {
    const std::uint32_t tok = t[o.at];
    if (o.type != kTypeTemp || index_dim(tok) != kIndex1D || !is_select1(tok)) return std::nullopt;
    if (op_extended(tok)) {
        if (!negate_allowed || o.size != 3 || t[o.at + 1] != kNegateModifier) return std::nullopt;
    } else if (o.size != 2) {
        return std::nullopt;
    }
    return TempComponent{o.at, o.size, o.reg, selected_component(tok)};
}

// The 2D constant-buffer operand cbB[R] (no modifier): its buffer and
// register.
struct CbRef {
    std::uint32_t buffer;
    std::uint32_t reg;
};
std::optional<CbRef> cb_ref(const std::uint32_t* t, const Operand& o) {
    const std::uint32_t tok = t[o.at];
    if (o.type != kTypeConstantBuffer || op_extended(tok) || index_dim(tok) != kIndex2D || o.size != 3)
        return std::nullopt;
    return CbRef{t[o.at + 1], t[o.at + 2]};
}

// ---- The instructions ------------------------------------------------------------

// The operands of an instruction, in order, or nothing when one cannot be
// parsed or they do not fill it exactly.
std::optional<std::array<Operand, 4>> operands4(const std::uint32_t* t, const Instruction& in) {
    std::array<Operand, 4> ops{};
    std::size_t p = in.at + 1;
    for (auto& o : ops) {
        const auto parsed = parse_operand(t, in.at + in.len, p);
        if (!parsed) return std::nullopt;
        o = *parsed;
        p = o.at + o.size;
    }
    if (p != in.at + in.len) return std::nullopt;
    return ops;
}

// A sprite corner instruction (24 §14.2): `mad rD.<c>, [-]rA.a, cb1[c].<c>,
// rC.c'` with c = x (register 0) or y (register 1).
struct Corner {
    std::size_t inst = 0; // index into the instruction list
    std::size_t len  = 0;
    Operand dst{};
    TempComponent offset{}; // a or b
    Operand projection{};
    TempComponent clip{};
};

std::optional<Corner> match_corner(const std::uint32_t* t, const Instruction& in, std::size_t index,
                                   std::uint32_t component) {
    if (in.op != kOpMad || in.extended) return std::nullopt;
    const auto ops = operands4(t, in);
    if (!ops) return std::nullopt;
    const auto& [dst, src0, src1, src2] = *ops;
    if (dst.type != kTypeTemp || dst.size != 2 || !is_masked(t[dst.at]) ||
        mask_of(t[dst.at]) != bit_of(component))
        return std::nullopt;
    const auto proj = cb_ref(t, src1);
    if (!proj || proj->buffer != kSpriteProjectionBuffer || proj->reg != component ||
        !is_select1(t[src1.at]) || selected_component(t[src1.at]) != component)
        return std::nullopt;
    const auto offset = temp_component(t, src0, true);
    const auto clip   = temp_component(t, src2, false);
    if (!offset || !clip) return std::nullopt;
    return Corner{index, in.len, dst, *offset, src1, *clip};
}

// Whether instruction `in` writes component `c` of temporary `reg`. Only
// the first operand is a destination except for the few two-result
// instructions, whose second is one too.
// The shader model 4 declarations, which write no temporary.
constexpr std::uint32_t kOpFirstDeclaration = 0x58, kOpLastDeclaration = 0x6A;

bool writes(const std::uint32_t* t, const Instruction& in, std::uint32_t reg, std::uint32_t c) {
    constexpr std::uint32_t kOpImul = 0x26, kOpSincos = 0x4D, kOpUdiv = 0x4E, kOpUmul = 0x51, kOpUaddc = 0x84,
                            kOpUsubb = 0x85, kOpSwapc = 0x8E;
    if (in.len == 1) return false; // no operands (ret, nop, flags)
    const bool two = in.op == kOpImul || in.op == kOpSincos || in.op == kOpUdiv || in.op == kOpUmul ||
                     in.op == kOpUaddc || in.op == kOpUsubb || in.op == kOpSwapc;
    std::size_t p  = in.at + 1;
    if (in.extended) {
        while (p < in.at + in.len && op_extended(t[p])) ++p; // chained extended opcode tokens
        ++p;
    }
    const int count = two ? 2 : 1;
    for (int k = 0; k < count; ++k) {
        const auto o = parse_operand(t, in.at + in.len, p);
        if (!o) return true; // unknown: assume it does
        if (o->type == kTypeTemp && index_dim(t[o->at]) == kIndex1D && o->reg == reg &&
            (!is_masked(t[o->at]) || (mask_of(t[o->at]) & bit_of(c)) != 0))
            return true;
        p = o->at + o->size;
    }
    return false;
}

// The clip coordinate a corner adds to was last written, before `corner`, by
// `dp4 rC.c', ..., cb0[R]` with R = `component` of ViewProj or
// SecondaryViewProj.
bool clip_from_view_proj(const std::uint32_t* t, const std::vector<Instruction>& insts, const Corner& corner,
                         std::uint32_t component) {
    for (std::size_t k = corner.inst; k-- > 0;) {
        const Instruction& in = insts[k];
        if (in.op == kOpCustomData || (in.op >= kOpFirstDeclaration && in.op <= kOpLastDeclaration)) continue;
        if (!writes(t, in, corner.clip.reg, corner.clip.component)) continue;
        if (in.op != kOpDp4 || in.extended) return false;
        std::size_t p = in.at + 1;
        std::optional<Operand> o;
        for (int n = 0; n < 3; ++n) {
            o = parse_operand(t, in.at + in.len, p);
            if (!o) return false;
            p = o->at + o->size;
        }
        const auto cb = cb_ref(t, *o);
        return cb && cb->buffer == kSpriteCameraBuffer &&
               (cb->reg == kViewProjRegister + component ||
                cb->reg == kSecondaryViewProjRegister + component);
    }
    return false;
}

// ---- The program -----------------------------------------------------------------

struct Site {
    Corner x;
    Corner y;
    std::size_t temps_decl = 0; // token index of dcl_temps
    std::uint32_t temps    = 0;
};

std::optional<Site> find_site(const std::vector<std::uint32_t>& t, std::size_t end) {
    const auto found = instructions(t, end);
    if (!found) return std::nullopt;
    const std::vector<Instruction>& insts = *found;

    Site s;
    std::size_t cb0_registers = 0;
    std::optional<Corner> cx, cy;
    for (std::size_t k = 0; k < insts.size(); ++k) {
        const Instruction& in = insts[k];
        if (in.op == kOpDclTemps && in.len == 2) {
            s.temps_decl = in.at;
            s.temps      = t[in.at + 1];
        } else if (in.op == kOpDclConstantBuffer && in.len == 4 &&
                   op_type(t[in.at + 1]) == kTypeConstantBuffer && t[in.at + 2] == kSpriteCameraBuffer) {
            cb0_registers = t[in.at + 3];
        } else if (auto c = match_corner(t.data(), in, k, kX)) {
            if (cx) return std::nullopt; // exactly one of each
            cx = c;
        } else if (auto d = match_corner(t.data(), in, k, kY)) {
            if (cy) return std::nullopt;
            cy = d;
        }
    }
    if (!cx || !cy || !s.temps_decl || cb0_registers <= kViewMatrixColumn1) return std::nullopt;
    if (!clip_from_view_proj(t.data(), insts, *cx, kX) || !clip_from_view_proj(t.data(), insts, *cy, kY))
        return std::nullopt;

    // Only dp4s between the two, and (a, b) computed before the first must
    // still be what each reads: nothing from the first up to the second
    // writes the second's offset.
    const Corner& first  = cx->inst < cy->inst ? *cx : *cy;
    const Corner& second = cx->inst < cy->inst ? *cy : *cx;
    for (std::size_t k = first.inst + 1; k < second.inst; ++k) {
        if (insts[k].op != kOpDp4 || insts[k].extended) return std::nullopt;
    }
    for (std::size_t k = first.inst; k < second.inst; ++k) {
        if (writes(t.data(), insts[k], second.offset.reg, second.offset.component)) return std::nullopt;
    }
    s.x = *cx;
    s.y = *cy;
    return s;
}

// ---- The rewrite -----------------------------------------------------------------

std::uint32_t f32(float v) { return std::bit_cast<std::uint32_t>(v); }

// Builds the inserted instructions of 24 §14.3 into `out`. T = r<temps>,
// S = r<temps + 1>:
//   T.x = cb0[4].y, T.y = cb0[5].y                      u
//   T.z = sqrt(dot(T.xy, T.xy)); T.z = max(L - T.z, 0)  ease
//   T.y += T.z; T.z = dot(T.xy, T.xy)                   w, |w|^2
//   T.w = T.z < eps; T.xyz = T.w ? (0, 1, 1) : T.xyz    degenerate
//   T.xy *= rsq(T.z)                                    n
//   S.x = b·n.x + a·n.y; S.y = b·n.y - a·n.x            (a', b')
class Emitter {
public:
    Emitter(std::vector<std::uint32_t>& out, std::uint32_t temp) : out_(out), t_(temp), s_(temp + 1) {}

    void emit(const std::uint32_t* src, const Site& site) {
        const auto a = std::span(src + site.x.offset.at, site.x.offset.size);
        const auto b = std::span(src + site.y.offset.at, site.y.offset.size);
        op(kOpMov, {dst(t_, kX), cb0(kViewMatrixColumn0)});
        op(kOpMov, {dst(t_, kY), cb0(kViewMatrixColumn1)});
        op(kOpDp2, {dst(t_, kZ), swz(t_, swizzle(kX, kY, kX, kX)), swz(t_, swizzle(kX, kY, kX, kX))});
        op(kOpSqrt, {dst(t_, kZ), sel(t_, kZ)});
        op(kOpAdd, {dst(t_, kZ), neg(sel(t_, kZ)), imm(f32(kUprightEaseLength))});
        op(kOpMax, {dst(t_, kZ), sel(t_, kZ), imm(f32(0.0f))});
        op(kOpAdd, {dst(t_, kY), sel(t_, kY), sel(t_, kZ)});
        op(kOpDp2, {dst(t_, kZ), swz(t_, swizzle(kX, kY, kX, kX)), swz(t_, swizzle(kX, kY, kX, kX))});
        op(kOpLt, {dst(t_, kW), sel(t_, kZ), imm(f32(kUprightDegenerateLength2))});
        op(kOpMovc, {dst_mask(t_, bit_of(kX) | bit_of(kY) | bit_of(kZ)), swz(t_, swizzle(kW, kW, kW, kW)),
                     imm4(f32(0.0f), f32(1.0f), f32(1.0f), f32(0.0f)), swz(t_, swizzle(kX, kY, kZ, kX))});
        op(kOpRsq, {dst(t_, kZ), sel(t_, kZ)});
        op(kOpMul, {dst_mask(t_, bit_of(kX) | bit_of(kY)), swz(t_, swizzle(kX, kY, kX, kX)),
                    swz(t_, swizzle(kZ, kZ, kZ, kZ))});
        op(kOpMul, {dst(s_, kX), copy(b), sel(t_, kX)});
        op(kOpMad, {dst(s_, kX), copy(a), sel(t_, kY), sel(s_, kX)});
        op(kOpMul, {dst(s_, kY), copy(a), sel(t_, kX)});
        op(kOpMad, {dst(s_, kY), copy(b), sel(t_, kY), neg(sel(s_, kY))});
    }

    // The corner `mad` with its offset source replaced by S.<component>.
    void corner(const std::uint32_t* src, const Corner& c, std::uint32_t component) {
        const std::size_t at     = c.dst.at - 1;
        const std::size_t before = out_.size();
        out_.push_back(0); // the opcode, sized below
        out_.insert(out_.end(), src + c.dst.at, src + c.dst.at + c.dst.size);
        const auto s = sel(s_, component);
        out_.insert(out_.end(), s.begin(), s.end());
        const std::size_t rest = c.offset.at + c.offset.size;
        out_.insert(out_.end(), src + rest, src + at + c.len);
        out_[before] = with_length(src[at], static_cast<std::uint32_t>(out_.size() - before));
    }

private:
    using Tokens = std::vector<std::uint32_t>;

    static Tokens dst(std::uint32_t reg, std::uint32_t c) { return dst_mask(reg, bit_of(c)); }
    static Tokens dst_mask(std::uint32_t reg, std::uint32_t mask) {
        return {masked_destination(mask, kTypeTemp, kIndex1D), reg};
    }
    static Tokens sel(std::uint32_t reg, std::uint32_t c) {
        return {selected_source(c, kTypeTemp, kIndex1D), reg};
    }
    static Tokens swz(std::uint32_t reg, std::uint32_t s) {
        return {swizzled_source(s, kTypeTemp, kIndex1D), reg};
    }
    static Tokens cb0(std::uint32_t reg) {
        return {selected_source(kY, kTypeConstantBuffer, kIndex2D), kSpriteCameraBuffer, reg};
    }
    static Tokens imm(std::uint32_t v) { return {kOneComponent | (kTypeImmediate32 << kTypeShift), v}; }
    static Tokens imm4(std::uint32_t x, std::uint32_t y, std::uint32_t z, std::uint32_t w) {
        return {kFourComponents | (kTypeImmediate32 << kTypeShift), x, y, z, w};
    }
    static Tokens neg(Tokens o) {
        o[0] |= kExtendedFlag;
        o.insert(o.begin() + 1, kNegateModifier);
        return o;
    }
    static Tokens copy(std::span<const std::uint32_t> o) { return {o.begin(), o.end()}; }

    void op(std::uint32_t code, std::initializer_list<Tokens> operands) {
        std::size_t len = 1;
        for (const auto& o : operands) len += o.size();
        out_.push_back(opcode_token(code, static_cast<std::uint32_t>(len)));
        for (const auto& o : operands) out_.insert(out_.end(), o.begin(), o.end());
    }

    std::vector<std::uint32_t>& out_;
    std::uint32_t t_;
    std::uint32_t s_;
};

// Two temporaries more for the inserted arithmetic.
constexpr std::uint32_t kAddedTemps = 2;

std::vector<std::uint32_t> rewrite(const std::vector<std::uint32_t>& t, const Site& site) {
    const Corner& first          = site.x.inst < site.y.inst ? site.x : site.y;
    const Corner& second         = site.x.inst < site.y.inst ? site.y : site.x;
    const std::uint32_t first_c  = &first == &site.x ? kX : kY;
    const std::uint32_t second_c = &first == &site.x ? kY : kX;
    const std::size_t first_at   = first.dst.at - 1;
    const std::size_t second_at  = second.dst.at - 1;

    std::vector<std::uint32_t> prog;
    prog.reserve(t.size() + 128);
    prog.insert(prog.end(), t.begin(), t.begin() + static_cast<std::ptrdiff_t>(first_at));
    prog[site.temps_decl + 1] = site.temps + kAddedTemps;
    Emitter e(prog, site.temps);
    e.emit(t.data(), site);
    e.corner(t.data(), first, first_c);
    prog.insert(prog.end(), t.begin() + static_cast<std::ptrdiff_t>(first_at + first.len),
                t.begin() + static_cast<std::ptrdiff_t>(second_at));
    e.corner(t.data(), second, second_c);
    prog.insert(prog.end(), t.begin() + static_cast<std::ptrdiff_t>(second_at + second.len), t.end());
    prog[1] = static_cast<std::uint32_t>(t[1] + (prog.size() - t.size()));
    return prog;
}

// The pre-filter: the x corner's projection operand, cb1[0].x, as it lies in
// the blob.
consteval std::array<std::uint8_t, 3 * kTokenBytes> projection_x_operand() {
    const std::array<std::uint32_t, 3> words = {selected_source(kX, kTypeConstantBuffer, kIndex2D),
                                                kSpriteProjectionBuffer, 0};
    std::array<std::uint8_t, 3 * kTokenBytes> bytes{};
    for (std::size_t i = 0; i < words.size(); ++i) {
        const auto w = std::bit_cast<std::array<std::uint8_t, kTokenBytes>>(words[i]);
        std::ranges::copy(w, bytes.begin() + static_cast<std::ptrdiff_t>(i * kTokenBytes));
    }
    return bytes;
}
constexpr auto kProjectionX = projection_x_operand();

std::vector<std::uint8_t> patch(std::span<const std::uint8_t> in, int& sites) {
    if (in.size() < kContainerHeaderSize || std::memcmp(in.data(), kDxbcMagic.data(), kDxbcMagicBytes) != 0)
        return {};
    // Most shaders have no sprite corner: skip them before the checksum.
    if (std::ranges::search(in, kProjectionX).empty()) return {};

    const auto chunk = find_program(in);
    if (!chunk) return {};
    const std::vector<std::uint32_t> t = program_tokens(in, *chunk);
    if (((t[0] >> kProgramTypeShift) & kProgramTypeMask) != kVertexShaderProgram) return {};
    if (t[1] < kProgramHeaderTokens || t[1] > t.size()) return {};

    const auto site = find_site(t, t[1]);
    if (!site) return {};
    const auto prog = rewrite(t, *site);
    auto out        = reassemble(in, *chunk, prog);
    sites           = 1;
    return out;
}

} // namespace

std::vector<std::uint8_t> patch_upright_sprites(const void* blob, std::size_t size, int& sites) noexcept {
    sites = 0;
    try {
        return patch({static_cast<const std::uint8_t*>(blob), size}, sites);
    } catch (...) {
        // An allocation failed: the original shader is used.
        sites = 0;
        return {};
    }
}

} // namespace omvr::core::render
