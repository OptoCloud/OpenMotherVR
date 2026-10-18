// OpenMotherVR-Provenance: clean
// core/render: the DXBC checksum against fxc's own on the project's mirror
// shaders, the position-dither fix on synthetic pixel shaders in the
// layouts of 08 §1.1, run through a small evaluator, and the particle
// lighting lookup in stereo.

#include "core/render/alpha_light.h"
#include "core/render/dither_fix.h"
#include "core/render/dxbc.h"

#include <doctest/doctest.h>

#include <array>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <initializer_list>
#include <span>
#include <utility>
#include <vector>

#include "platform/d3d11/render/shaders/mirror_shaders.inc"

namespace {

using namespace omvr::core::render;

std::uint32_t u32_at(std::span<const std::uint8_t> b, std::size_t at) {
    std::uint32_t v = 0;
    std::memcpy(&v, b.data() + at, sizeof(v));
    return v;
}

void check_stored_checksum(std::span<const std::uint8_t> blob) {
    const DxbcChecksum sum = dxbc_checksum(blob);
    for (std::size_t i = 0; i < sum.size(); ++i) CHECK(sum[i] == u32_at(blob, 4 + 4 * i));
}

TEST_CASE("the DXBC checksum equals the one fxc stored") {
    check_stored_checksum({kMirrorVS, sizeof(kMirrorVS)});
    check_stored_checksum({kMirrorPS, sizeof(kMirrorPS)});
}

TEST_CASE("the dither fix leaves shaders without the pattern alone") {
    const DitherPatch mirror = patch_position_dither(kMirrorPS, sizeof(kMirrorPS));
    CHECK(mirror.blob.empty());
    CHECK(mirror.sites == 0);
    CHECK_FALSE(mirror.has_constants);
    const std::array<std::uint8_t, 8> junk = {1, 2, 3, 4, 5, 6, 7, 8};
    CHECK(patch_position_dither(junk.data(), junk.size()).blob.empty());
}

// 0.001f, the scale every dither site multiplies the pixel position by.
constexpr std::uint32_t kPoint001 = 0x3A83126F;

// A ps_5_0 program in the layout of 08 §1.1's table: dcl cb2[10];
// mul r0.xy, v0.xyxx, l(0.001, 0.001, 0, 0);
// mad r1.zw, v0.xxxy, l(0, 0, 0.001, 0.001), -v1.xxxx;
// dp2 r2.x, r1.zwzz, r0.xyxx; ret.
std::vector<std::uint32_t> dither_program() {
    return {
        0x00000050, 37,                                           // ps_5_0, 37 tokens
        0x04000059, 0x00208E46, 2,         10,                    // dcl_constantbuffer cb2[10]
        0x0A000038, 0x00100032, 0,         0x00101046, 0,         // mul r0.xy, v0.xyxx,
        0x00004002, kPoint001,  kPoint001, 0,          0,         //     l(0.001, 0.001, 0, 0)
        0x0D000032, 0x001000C2, 1,         0x00101406, 0,         // mad r1.zw, v0.xxxy,
        0x00004002, 0,          0,         kPoint001,  kPoint001, //     l(0, 0, 0.001, 0.001),
        0x80101006, 0x00000041, 1,                                //     -v1.xxxx
        0x0700000F, 0x00100012, 2,         0x00100AE6, 1,         // dp2 r2.x, r1.zwzz,
        0x00100046, 0,                                            //     r0.xyxx
        0x0100003E,                                               // ret
    };
}

// A DXBC container with one SHEX chunk holding `program`, checksummed.
std::vector<std::uint8_t> container(const std::vector<std::uint32_t>& program) {
    constexpr std::size_t kChunkAt = 36;
    const std::size_t bytes        = program.size() * 4;
    std::vector<std::uint8_t> out(kChunkAt + 8 + bytes);
    const auto put = [&out](std::size_t at, std::uint32_t v) { std::memcpy(out.data() + at, &v, sizeof(v)); };
    std::memcpy(out.data(), "DXBC", 4);
    put(20, 1);
    put(24, static_cast<std::uint32_t>(out.size()));
    put(28, 1);
    put(32, kChunkAt);
    std::memcpy(out.data() + kChunkAt, "SHEX", 4);
    put(kChunkAt + 4, static_cast<std::uint32_t>(bytes));
    std::memcpy(out.data() + kChunkAt + 8, program.data(), bytes);
    const DxbcChecksum sum = dxbc_checksum(out);
    std::memcpy(out.data() + 4, sum.data(), 16);
    return out;
}

// The program tokens of a container built by container().
std::vector<std::uint32_t> program_of(const std::vector<std::uint8_t>& blob) {
    std::vector<std::uint32_t> t((blob.size() - 44) / 4);
    std::memcpy(t.data(), blob.data() + 44, t.size() * 4);
    return t;
}

TEST_CASE("the dither fix rewrites one site and keeps the container valid") {
    const std::vector<std::uint8_t> in   = container(dither_program());
    const DitherPatch patched            = patch_position_dither(in.data(), in.size());
    const std::vector<std::uint8_t>& out = patched.blob;
    REQUIRE(patched.sites == 1);
    CHECK(patched.has_constants);
    REQUIRE(out.size() == in.size() + 32); // 8 tokens more per site
    CHECK(u32_at(out, 24) == out.size());
    CHECK(u32_at(out, 36 + 4) == 37 * 4 + 32);
    check_stored_checksum(out);

    const std::vector<std::uint32_t> t = program_of(out);
    CHECK(t[1] == 45u);         // program length
    CHECK(t[5] == 40u);         // cb2 now declares register 39
    CHECK(t[6] == 0x08000038u); // mul r0.xy, v0.xyxx, cb2[39].zwzz
    CHECK(t[11] == 0x00208AE6u);
    CHECK(t[12] == 2u);
    CHECK(t[13] == 39u);
    CHECK(t[14] == 0x0D000032u); // the mad, reading r0 in place of v0
    CHECK(t[17] == 0x00100406u);
    CHECK(t[18] == 0u);
    CHECK(t[27] == 0x0A000038u); // mul r0.xy, r0.xyzw, l(0.001, 0.001, 0, 0)
    CHECK(t[30] == 0x00100E46u);
    CHECK(t[33] == kPoint001);
    CHECK(t[37] == 0x0700000Fu); // the dp2, unchanged
    CHECK(t[44] == 0x0100003Eu); // ret
}

// ---- Shader model 5 arithmetic, as the device runs it ---------------------------
//
// Enough of the public token format to run the synthetic programs: mul, mad,
// dp2, frc, add and ret on temporaries, inputs, immediates and cb2, with
// swizzles, write masks and the negate modifier. Written independently of
// the patcher, so the tests check what the patched program computes.

using Reg4 = std::array<float, 4>;

struct Machine {
    std::array<Reg4, 8> r{};
    std::array<Reg4, 4> v{};
    std::array<Reg4, 40> cb2{};

    // Reads the source operand at t[i] into `out`; returns its tokens.
    std::size_t source(const std::vector<std::uint32_t>& t, std::size_t i, Reg4& out) const {
        const std::uint32_t tok  = t[i];
        const std::uint32_t type = (tok >> 12) & 0xFF;
        std::size_t p            = i + 1;
        bool negate              = false;
        if (tok & 0x80000000u) negate = t[p++] == 0x41;
        Reg4 raw{};
        if (type == 4) {
            for (std::size_t c = 0; c < 4; ++c) std::memcpy(&raw[c], &t[p + c], 4);
            p += 4;
        } else if (type == 8) {
            REQUIRE(t[p] == 2);
            raw = cb2.at(t[p + 1]);
            p += 2;
        } else {
            raw = type == 0 ? r.at(t[p]) : v.at(t[p]);
            p += 1;
        }
        const std::uint32_t swizzle = type == 4 ? 0xE4 : (tok >> 4) & 0xFF;
        for (std::size_t c = 0; c < 4; ++c) {
            out[c] = raw[(swizzle >> (2 * c)) & 3];
            if (negate) out[c] = -out[c];
        }
        return p - i;
    }

    static float apply(std::uint32_t op, const std::array<Reg4, 3>& s, std::size_t c) {
        switch (op) {
            case 0x00: return s[0][c] + s[1][c];                     // add
            case 0x38: return s[0][c] * s[1][c];                     // mul
            case 0x32: return s[0][c] * s[1][c] + s[2][c];           // mad
            case 0x0F: return s[0][0] * s[1][0] + s[0][1] * s[1][1]; // dp2
            case 0x1A: return s[0][c] - std::floor(s[0][c]);         // frc
            default: FAIL("unexpected opcode ", op); return 0.0f;
        }
    }

    void run(const std::vector<std::uint32_t>& t) {
        for (std::size_t i = 2; i < t[1];) {
            const std::uint32_t op  = t[i] & 0x7FF;
            const std::uint32_t len = (t[i] >> 24) & 0x7F;
            if (op == 0x3E) return; // ret
            if (op != 0x59) {       // dcl_constantbuffer: nothing to run
                const std::uint32_t mask = (t[i + 1] >> 4) & 0xF;
                std::array<Reg4, 3> s{};
                std::size_t p           = i + 3;
                const std::size_t count = op == 0x32 ? 3 : op == 0x1A ? 1 : 2;
                for (std::size_t k = 0; k < count; ++k) p += source(t, p, s[k]);
                REQUIRE(p == i + len);
                Reg4 res{};
                for (std::size_t c = 0; c < 4; ++c) res[c] = apply(op, s, c);
                Reg4& dst = r.at(t[i + 2]);
                for (std::size_t c = 0; c < 4; ++c) {
                    if (mask & (1u << c)) dst[c] = res[c];
                }
            }
            i += len;
        }
    }
};

// Builds the layouts of 08 §1.1.
constexpr std::uint32_t kX = 0, kY = 1, kZ = 2, kW = 3;
constexpr std::uint32_t swz(std::uint32_t x, std::uint32_t y, std::uint32_t z, std::uint32_t w) {
    return x | (y << 2) | (z << 4) | (w << 6);
}
using Tokens = std::vector<std::uint32_t>;
Tokens dst(std::uint32_t reg, std::uint32_t mask) { return {0x00100002 | (mask << 4), reg}; }
Tokens temp(std::uint32_t reg, std::uint32_t s) { return {0x00100006 | (s << 4), reg}; }
Tokens input(std::uint32_t reg, std::uint32_t s) { return {0x00101006 | (s << 4), reg}; }
Tokens neg_input(std::uint32_t reg, std::uint32_t s) { return {0x80101006 | (s << 4), 0x41, reg}; }
Tokens imm(std::uint32_t x, std::uint32_t y, std::uint32_t z, std::uint32_t w) {
    return {0x00004002, x, y, z, w};
}

struct Program {
    Tokens t{0x00000050, 0};

    Program& op(std::uint32_t code, std::initializer_list<Tokens> operands) {
        std::size_t len = 1;
        for (const auto& o : operands) len += o.size();
        t.push_back(code | static_cast<std::uint32_t>(len << 24));
        for (const auto& o : operands) t.insert(t.end(), o.begin(), o.end());
        return *this;
    }
    Program& dcl_cb2(std::uint32_t registers) {
        t.insert(t.end(), {0x04000059, 0x00208E46, 2, registers});
        return *this;
    }
    Tokens done() {
        t.push_back(0x0100003E);
        t[1] = static_cast<std::uint32_t>(t.size());
        return t;
    }
};

constexpr std::uint32_t kMul = 0x38, kMad = 0x32, kDp2 = 0x0F, kAdd = 0x00;
constexpr std::uint32_t K = kPoint001;

// 08 §1.1 r7's example of another layout (980 sites):
// mul rA.yz, vP.xxyx, l(0, 0.001, 0.001, 0);
// mad rB.xy, vP.xyxx, l(0.001, 0.001, 0, 0), -vT.xxxx;
// dp2 rC.y, rB.xyxx, rA.yzyy; with rA = r3, rB = r1, rC = r2, vP = v0,
// vT = v1. With cb2, the mul is at token 6, the mad at 16 and the dp2 at 29.
Program yz_then_xy(bool cb2) {
    Program p;
    if (cb2) p.dcl_cb2(40);
    p.op(kMul, {dst(3, 0b0110), input(0, swz(kX, kX, kY, kX)), imm(0, K, K, 0)});
    p.op(kMad,
         {dst(1, 0b0011), input(0, swz(kX, kY, kX, kX)), imm(K, K, 0, 0), neg_input(1, swz(kX, kX, kX, kX))});
    p.op(kDp2, {dst(2, 0b0010), temp(1, swz(kX, kY, kX, kX)), temp(3, swz(kY, kZ, kY, kY))});
    return p;
}

// Two disjoint pairs of one temporary, the mul's pair the second:
// mul r4.zw, v0.xxxy, l(0, 0, 0.001, 0.001);
// mad r4.xy, v0.xyxx, l(0.001, 0.001, 0, 0), -v2.yyyy;
// dp2 r5.w, r4.zwzz, r4.xyxx.
Program one_temp_zw_then_xy() {
    Program p;
    p.op(kMul, {dst(4, 0b1100), input(0, swz(kX, kX, kX, kY)), imm(0, 0, K, K)});
    p.op(kMad,
         {dst(4, 0b0011), input(0, swz(kX, kY, kX, kX)), imm(K, K, 0, 0), neg_input(2, swz(kY, kY, kY, kY))});
    p.op(kDp2, {dst(5, 0b1000), temp(4, swz(kZ, kW, kZ, kZ)), temp(4, swz(kX, kY, kX, kX))});
    return p;
}

// Runs `prog` at pixel (x, y) of a w x h target, with the texture
// coordinate `tc` in every component of v1 and v2.
Machine run_at(const Tokens& prog, float x, float y, float tc, float w, float h) {
    Machine m;
    for (auto& reg : m.r) reg = {7.0f, 7.0f, 7.0f, 7.0f}; // values the rest of a shader may read
    m.v[0]       = {x, y, 0.25f, 1.0f};
    m.v[1]       = {tc, tc, tc, tc};
    m.v[2]       = {tc, tc, tc, tc};
    m.cb2.at(39) = {w, h, 1.0f / w, 1.0f / h};
    m.run(prog);
    return m;
}

// The dot product of 08 §1.2 for the position n the hash works on:
// d = (0.001 n - t).(0.001 n).
float spec_d(float nx, float ny, float tc) {
    const float ax = 0.001f * nx, ay = 0.001f * ny;
    return (ax - tc) * ax + (ay - tc) * ay;
}

// frc(p / 1024), the wrapped position of 08 §1.4 r7.
float wrapped(float p) { return p / 1024.0f - std::floor(p / 1024.0f); }

// 08 §1.4 "No live value overwritten": every register component other than
// those listed is as the original program left it.
void check_untouched(const Machine& before, const Machine& after,
                     std::initializer_list<std::pair<std::uint32_t, std::uint32_t>> results) {
    for (std::uint32_t reg = 0; reg < before.r.size(); ++reg) {
        for (std::uint32_t c = 0; c < 4; ++c) {
            bool result = false;
            for (const auto& [r, mask] : results) result = result || (reg == r && (mask & (1u << c)) != 0);
            if (!result) CHECK(after.r[reg][c] == before.r[reg][c]);
        }
    }
}

// An Index at pixel scale 2.0: a side-by-side target of 10752 x 5376 (08 §1.5).
constexpr float kWidth = 10752.0f, kHeight = 5376.0f;
constexpr std::array kPixels    = {std::pair{0.5f, 0.5f}, std::pair{5375.5f, 2687.5f},
                                   std::pair{10751.5f, 5375.5f}, std::pair{8000.5f, 100.5f},
                                   std::pair{1023.5f, 1024.5f}};
constexpr std::array kTexcoords = {-1000.0f, -5.0f, -2.0f, 0.0f, 0.5f, 1000.0f};

TEST_CASE("the table's layout: the patched hash input is that of p / size, bounded") {
    const Tokens prog = dither_program();
    const auto in     = container(prog);
    const Tokens t    = program_of(patch_position_dither(in.data(), in.size()).blob);
    for (const float tc : kTexcoords) {
        for (const auto [x, y] : kPixels) {
            const Machine a = run_at(prog, x, y, tc, kWidth, kHeight);
            const Machine b = run_at(t, x, y, tc, kWidth, kHeight);
            CHECK(a.r[2][kX] == doctest::Approx(spec_d(x, y, tc)).epsilon(1e-4)); // the game's d
            CHECK(b.r[2][kX] == doctest::Approx(spec_d(x / kWidth, y / kHeight, tc)).epsilon(1e-4));
            CHECK(std::abs(b.r[2][kX]) <= 8.0f);
            check_untouched(a, b, {{0, 0b0011}, {1, 0b1100}, {2, 0b0001}});
        }
    }
}

TEST_CASE("another layout (yz then xy) is patched with cb2[39]") {
    const Tokens prog = yz_then_xy(true).done();
    const auto in     = container(prog);
    const auto out    = patch_position_dither(in.data(), in.size());
    REQUIRE(out.sites == 1);
    REQUIRE(out.blob.size() == in.size() + 32);
    CHECK(u32_at(out.blob, 24) == out.blob.size());
    check_stored_checksum(out.blob);
    const Tokens t = program_of(out.blob);
    CHECK(t[6] == 0x08000038u); // mul r3.yz, v0.xxyx, cb2[39].zzwz
    CHECK(t[7] == 0x00100062u);
    CHECK(t[11] == (0x00208006u | (swz(kZ, kZ, kW, kZ) << 4)));
    CHECK(t[17] == (0x00100006u | (swz(kY, kZ, kY, kY) << 4))); // the mad reads r3.yzyy
    CHECK(t[18] == 3u);
    for (const float tc : kTexcoords) {
        for (const auto [x, y] : kPixels) {
            const Machine a = run_at(prog, x, y, tc, kWidth, kHeight);
            const Machine b = run_at(t, x, y, tc, kWidth, kHeight);
            CHECK(a.r[2][kY] == doctest::Approx(spec_d(x, y, tc)).epsilon(1e-4));
            CHECK(b.r[2][kY] == doctest::Approx(spec_d(x / kWidth, y / kHeight, tc)).epsilon(1e-4));
            CHECK(std::abs(b.r[2][kY]) <= 8.0f);
            check_untouched(a, b, {{3, 0b0110}, {1, 0b0011}, {2, 0b0010}});
        }
    }
}

TEST_CASE("a shader without cb2 is patched with the position wrapped to 1024 pixels") {
    const Tokens prog = yz_then_xy(false).done();
    const auto in     = container(prog);
    const auto out    = patch_position_dither(in.data(), in.size());
    REQUIRE(out.sites == 1);
    REQUIRE(out.blob.size() == in.size() + 60); // 15 tokens more, and no constant buffer
    check_stored_checksum(out.blob);
    const Tokens t = program_of(out.blob);
    for (std::size_t i = 2; i < t.size(); ++i) CHECK((t[i] & 0x7FF) != 0x59u);
    for (const float tc : kTexcoords) {
        for (const auto [x, y] : kPixels) {
            const Machine a = run_at(prog, x, y, tc, kWidth, kHeight);
            const Machine b = run_at(t, x, y, tc, kWidth, kHeight);
            CHECK(b.r[2][kY] == doctest::Approx(spec_d(wrapped(x), wrapped(y), tc)).epsilon(1e-4));
            CHECK(std::abs(b.r[2][kY]) <= 8.0f);
            check_untouched(a, b, {{3, 0b0110}, {1, 0b0011}, {2, 0b0010}});
        }
    }
}

TEST_CASE("two pairs of one temporary are patched, and the cb2 declared after them raised") {
    Program p = one_temp_zw_then_xy();
    p.op(kAdd, {dst(6, 0b0001), temp(5, swz(kW, kW, kW, kW)), temp(4, swz(kX, kX, kX, kX))});
    const Tokens prog = p.dcl_cb2(12).done();
    const auto in     = container(prog);
    const auto out    = patch_position_dither(in.data(), in.size());
    REQUIRE(out.sites == 1);
    const Tokens t = program_of(out.blob);
    CHECK(t[t[1] - 2] == 40u);
    for (const float tc : kTexcoords) {
        const Machine a = run_at(prog, 10000.5f, 5000.5f, tc, kWidth, kHeight);
        const Machine b = run_at(t, 10000.5f, 5000.5f, tc, kWidth, kHeight);
        CHECK(b.r[5][kW] == doctest::Approx(spec_d(10000.5f / kWidth, 5000.5f / kHeight, tc)).epsilon(1e-4));
        check_untouched(a, b, {{4, 0b1111}, {5, 0b1000}, {6, 0b0001}});
        CHECK(b.r[6][kX] == b.r[5][kW] + b.r[4][kX]); // what follows reads the new values
    }
}

TEST_CASE("the patched hash input still varies between neighbouring pixels") {
    const auto in  = container(dither_program());
    const Tokens t = program_of(patch_position_dither(in.data(), in.size()).blob);
    const float d0 = run_at(t, 4000.5f, 1000.5f, 0.75f, kWidth, kHeight).r[2][kX];
    CHECK(d0 != run_at(t, 4001.5f, 1000.5f, 0.75f, kWidth, kHeight).r[2][kX]);
    CHECK(d0 != run_at(t, 4000.5f, 1001.5f, 0.75f, kWidth, kHeight).r[2][kX]);
}

TEST_CASE("the dither fix refuses forms that are not a dither site") {
    const Tokens base  = yz_then_xy(true).done();
    const auto changed = [&base](auto&& edit) {
        Tokens t = base;
        edit(t);
        const auto in = container(t);
        return patch_position_dither(in.data(), in.size());
    };
    const auto refused = [&changed](auto&& edit) {
        const DitherPatch r = changed(edit);
        CHECK(r.blob.empty());
        CHECK(r.sites == 0);
    };
    REQUIRE(changed([](Tokens&) {}).sites == 1);
    // The mul writes three components.
    refused([](Tokens& t) { t[7] = 0x00100072; });
    // 0.001 in a component the mul does not write.
    refused([](Tokens& t) { t[15] = K; });
    // vP.y in the first written component of the mul.
    refused([](Tokens& t) { t[9] = 0x00101006 | (swz(kX, kY, kX, kX) << 4); });
    // A saturated mul.
    refused([](Tokens& t) { t[6] |= 0x2000; });
    // The texture coordinate not negated (the mad one token shorter).
    refused([](Tokens& t) {
        t[16] -= 1u << 24;
        t[26] = 0x00101006;
        t.erase(t.begin() + 27);
        t[1] -= 1;
    });
    // Two different components of the texture coordinate.
    refused([](Tokens& t) { t[26] = 0x80101006 | (swz(kX, kY, kX, kX) << 4); });
    // The mad reads another input than the mul's vP.
    refused([](Tokens& t) { t[20] = 2; });
    // The mad writes a component the mul wrote.
    refused([](Tokens& t) {
        t[17] = 0x00100032;
        t[18] = 3;
        t[33] = 3; // the dp2 reads r3.xy
    });
    // The dp2 reads the mul's pair in the other order.
    refused([](Tokens& t) { t[34] = 0x00100006 | (swz(kZ, kY, kZ, kZ) << 4); });
    // An add in place of the dp2.
    refused([](Tokens& t) { t[29] = 0x07000000; });
}

TEST_CASE("the dither fix refuses a pair with another instruction between") {
    Program p;
    p.dcl_cb2(40);
    p.op(kMul, {dst(3, 0b0110), input(0, swz(kX, kX, kY, kX)), imm(0, K, K, 0)});
    p.op(kAdd, {dst(7, 0b0001), temp(7, swz(kX, kX, kX, kX)), temp(7, swz(kX, kX, kX, kX))});
    p.op(kMad,
         {dst(1, 0b0011), input(0, swz(kX, kY, kX, kX)), imm(K, K, 0, 0), neg_input(1, swz(kX, kX, kX, kX))});
    p.op(kDp2, {dst(2, 0b0010), temp(1, swz(kX, kY, kX, kX)), temp(3, swz(kY, kZ, kY, kY))});
    const auto in       = container(p.done());
    const DitherPatch r = patch_position_dither(in.data(), in.size());
    CHECK(r.blob.empty());
    CHECK(r.has_constants); // counted as holding the dither's constants (11 §2.2 r8)
}

TEST_CASE("the dither fix refuses what it does not understand") {
    std::vector<std::uint8_t> bad_sum = container(dither_program());
    bad_sum[4] ^= 1;
    const DitherPatch bad = patch_position_dither(bad_sum.data(), bad_sum.size());
    CHECK(bad.blob.empty());
    CHECK(bad.has_constants);

    std::vector<std::uint32_t> vertex  = dither_program();
    vertex[0]                          = 0x00010050; // vs_5_0
    const std::vector<std::uint8_t> vs = container(vertex);
    CHECK(patch_position_dither(vs.data(), vs.size()).blob.empty());
}

// 08 §8.1, §8.3: the game addresses the lighting atlas with
// `ndc / scale - offset` of the drawn eye; it must land on the centre's NDC
// `s_c * t + o_c` of the same view tangent t.
float lookup(float ndc, float scale, float offset) { return ndc / scale - offset; }

TEST_CASE("flat play keeps the game's own lookup") {
    const NdcScaleOffset flat{};
    const NdcScaleOffset r = eye_ndc_in_centre(flat, flat);
    CHECK(r.offset_x == 0.0f);
    CHECK(r.offset_y == 0.0f);
    CHECK(r.scale_x == 1.0f);
    CHECK(r.scale_y == 1.0f);
}

TEST_CASE("an eye with the centre's symmetric FOV reads the atlas at its own NDC") {
    // The mod's render FOV (03 §3.2): both eyes and the centre share one
    // symmetric FovPort, tangents 1.4 wide and 1.25 high.
    const NdcScaleOffset shared{
        .offset_x = 0.0f, .offset_y = 0.0f, .scale_x = 1.0f / 1.4f, .scale_y = 1.0f / 1.25f};
    const NdcScaleOffset r = eye_ndc_in_centre(shared, shared);
    CHECK(r.offset_x == doctest::Approx(0.0f));
    CHECK(r.offset_y == doctest::Approx(0.0f));
    CHECK(r.scale_x == doctest::Approx(1.0f));
    CHECK(r.scale_y == doctest::Approx(1.0f));
}

TEST_CASE("an eye with its own FOV reads the atlas where the centre put that direction") {
    // LibOVR's FovPort to NDC (slot 14): scale 2 / (l + r), offset
    // (l - r) * scale / 2; the centre (eye -1) has no offset.
    const float l = 1.1f, rt = 1.4f, u = 1.3f, d = 1.2f;
    const NdcScaleOffset eye{.offset_x = (l - rt) / (l + rt),
                             .offset_y = (u - d) / (u + d),
                             .scale_x  = 2.0f / (l + rt),
                             .scale_y  = 2.0f / (u + d)};
    const NdcScaleOffset centre{.offset_x = 0.0f, .offset_y = 0.0f, .scale_x = 0.8f, .scale_y = 0.75f};
    const NdcScaleOffset r = eye_ndc_in_centre(eye, centre);
    for (const float t : {-1.0f, -0.3f, 0.0f, 0.5f, 1.2f}) {
        const float eye_x = eye.scale_x * t + eye.offset_x;
        const float eye_y = eye.scale_y * t + eye.offset_y;
        CHECK(lookup(eye_x, r.scale_x, r.offset_x) == doctest::Approx(centre.scale_x * t));
        CHECK(lookup(eye_y, r.scale_y, r.offset_y) == doctest::Approx(centre.scale_y * t));
    }
}

TEST_CASE("a lookup without a usable scale stays the game's") {
    const NdcScaleOffset eye{.offset_x = 0.1f, .offset_y = 0.0f, .scale_x = 0.7f, .scale_y = 0.8f};
    const NdcScaleOffset zero{.offset_x = 0.0f, .offset_y = 0.0f, .scale_x = 0.0f, .scale_y = 0.8f};
    const NdcScaleOffset r = eye_ndc_in_centre(eye, zero);
    CHECK(r.offset_x == eye.offset_x);
    CHECK(r.scale_x == eye.scale_x);
    CHECK(r.scale_y == eye.scale_y);
}

TEST_CASE("the particle lighting trace reports each eye's answer once per change") {
    AlphaLightTrace trace;
    const NdcScaleOffset a{.offset_x = 0.0f, .offset_y = 0.0f, .scale_x = 0.7f, .scale_y = 0.8f};
    const NdcScaleOffset b{.offset_x = 0.0f, .offset_y = 0.0f, .scale_x = 0.6f, .scale_y = 0.8f};
    CHECK(trace.changed(0, a));
    CHECK_FALSE(trace.changed(0, a));
    CHECK(trace.changed(1, a));
    CHECK_FALSE(trace.changed(1, a));
    CHECK(trace.changed(0, b));
    CHECK_FALSE(trace.changed(1, a));
}

} // namespace
