// OpenMotherVR-Provenance: clean
// Upright sprites in stereo (24 §14): the roll removal of the sprite corner
// and the vertex shader rewrite, run on a synthetic shader of the game's
// form (24 §14.2) by a small interpreter of the instructions involved.
// Expected values come from the geometry of 24 §14.3: a rolled camera's
// view of the world's axes.

#include "core/render/dxbc.h"
#include "core/render/upright_sprites.h"

#include <doctest/doctest.h>

#include <array>
#include <bit>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <numbers>
#include <span>
#include <vector>

namespace {

using namespace omvr::core::render;
using omvr::util::Vec2;
using omvr::util::Vec3;

constexpr float kEps = 1e-5f;

float dot3(const Vec3& a, const Vec3& b) { return a.x * b.x + a.y * b.y + a.z * b.z; }

// A camera rolled by `roll` about a forward axis pitched by `pitch` (radians,
// positive up), facing +Z at zero pitch: its right, up and forward axes in
// world space (left-handed, +Y up, 05 §1).
struct Camera {
    Vec3 right, up, forward;
};
Camera camera(float pitch, float roll) {
    const float cp = std::cos(pitch), sp = std::sin(pitch), cr = std::cos(roll), sr = std::sin(roll);
    const Vec3 r0{1, 0, 0};
    const Vec3 u0{0, cp, -sp};
    const Vec3 f{0, sp, cp};
    // Roll: right and up turn in their plane.
    return {{cr * r0.x + sr * u0.x, cr * r0.y + sr * u0.y, cr * r0.z + sr * u0.z},
            {-sr * r0.x + cr * u0.x, -sr * r0.y + cr * u0.y, -sr * r0.z + cr * u0.z},
            f};
}

void check_vec(const Vec3& a, const Vec3& b) {
    CHECK(a.x == doctest::Approx(b.x).epsilon(kEps));
    CHECK(a.y == doctest::Approx(b.y).epsilon(kEps));
    CHECK(a.z == doctest::Approx(b.z).epsilon(kEps));
}

TEST_CASE("an upright camera's sprites are the game's own") {
    const Camera c     = camera(0.3f, 0.0f);
    const SpriteAxes s = upright_sprite_axes(c.right, c.up, c.forward);
    check_vec(s.right, c.right);
    check_vec(s.up, c.up);
    check_vec(s.forward, c.forward);
    const Vec2 n = sprite_up_in_view(0.0f, 0.8f);
    CHECK(n.x == 0.0f);
    CHECK(n.y == 1.0f);
    const Vec2 o = upright_offset({0.25f, -0.5f}, n);
    CHECK(o.x == 0.25f);
    CHECK(o.y == -0.5f);
}

TEST_CASE("a rolled camera's sprites keep the view's forward and the world's up") {
    for (const float pitch : {0.0f, 0.5f, -0.9f}) {
        for (const float roll : {0.2f, -0.6f, 1.2f, 3.0f}) {
            const Camera c     = camera(pitch, roll);
            const SpriteAxes s = upright_sprite_axes(c.right, c.up, c.forward);
            check_vec(s.forward, c.forward);
            // Still in the view plane, still an orthonormal pair.
            CHECK(dot3(s.right, c.forward) == doctest::Approx(0.0f).epsilon(kEps));
            CHECK(dot3(s.up, c.forward) == doctest::Approx(0.0f).epsilon(kEps));
            CHECK(dot3(s.right, s.up) == doctest::Approx(0.0f).epsilon(kEps));
            CHECK(dot3(s.right, s.right) == doctest::Approx(1.0f).epsilon(kEps));
            // Upright: the right axis is horizontal, the up axis points up
            // and is the unrolled camera's up.
            CHECK(s.right.y == doctest::Approx(0.0f).epsilon(kEps));
            CHECK(s.up.y > 0.0f);
            const Camera level = camera(pitch, 0.0f);
            check_vec(s.up, level.up);
            check_vec(s.right, level.right);
        }
    }
}

TEST_CASE("a rolled view turns the corner offset to the world's axes") {
    // A camera rolled by 30 degrees with no pitch sees the world's +X at
    // view (cos 30, -sin 30) and the world's up at view (sin 30, cos 30): a
    // sprite's unit right offset must land on the first, its up on the
    // second.
    const float r    = std::numbers::pi_v<float> / 6.0f;
    const Camera c   = camera(0.0f, r);
    const Vec2 n     = sprite_up_in_view(c.right.y, c.up.y);
    const Vec2 right = upright_offset({1.0f, 0.0f}, n);
    const Vec2 up    = upright_offset({0.0f, 1.0f}, n);
    CHECK(right.x == doctest::Approx(std::cos(r)).epsilon(kEps));
    CHECK(right.y == doctest::Approx(-std::sin(r)).epsilon(kEps));
    CHECK(up.x == doctest::Approx(std::sin(r)).epsilon(kEps));
    CHECK(up.y == doctest::Approx(std::cos(r)).epsilon(kEps));
}

TEST_CASE("looking straight up or down keeps the game's own orientation") {
    for (const float pitch : {std::numbers::pi_v<float> / 2, -std::numbers::pi_v<float> / 2}) {
        for (const float roll : {0.0f, 0.7f, -2.0f}) {
            const Camera c     = camera(pitch, roll);
            const SpriteAxes s = upright_sprite_axes(c.right, c.up, c.forward);
            check_vec(s.right, c.right);
            check_vec(s.up, c.up);
            CHECK(std::isfinite(s.up.x));
        }
    }
    // An upside-down view near the vertical: the eased up has no direction.
    const Vec2 flipped = sprite_up_in_view(0.0f, -0.05f);
    CHECK(flipped.x == 0.0f);
    CHECK(flipped.y == 1.0f);
    const Vec2 none = sprite_up_in_view(0.0f, 0.0f);
    CHECK(none.y == 1.0f);
    const Vec2 nan = sprite_up_in_view(std::nanf(""), 0.0f);
    CHECK(nan.y == 1.0f);
}

TEST_CASE("near the vertical the sprites ease back without a jump") {
    // 24 §14.3 item 2, L = 0.1: at and beyond L, u's own direction; at half
    // of L with the world's up along view x, half way: w = (0.05, 0.05).
    const Vec2 at = sprite_up_in_view(0.1f, 0.0f);
    CHECK(at.x == doctest::Approx(1.0f));
    CHECK(at.y == doctest::Approx(0.0f));
    const Vec2 below = sprite_up_in_view(0.0999f, 0.0f);
    CHECK(below.x == doctest::Approx(1.0f).epsilon(1e-3));
    const Vec2 half = sprite_up_in_view(0.05f, 0.0f);
    CHECK(half.x == doctest::Approx(std::sqrt(0.5f)).epsilon(kEps));
    CHECK(half.y == doctest::Approx(std::sqrt(0.5f)).epsilon(kEps));
}

// ---- The shader rewrite ----------------------------------------------------------

// A vs_5_0 program of the game's sprite form (24 §14.2):
//   dcl_constantbuffer cb0[23]; dcl_constantbuffer cb1[2]; dcl_temps 3
//   dp4 r0.z, r2.xyzw, cb0[0].xyzw
//   mad r1.x, r0.y, cb1[0].x, r0.z        ; a = r0.y
//   dp4 r0.y, r2.xyzw, cb0[1].xyzw
//   mad r1.y, -r0.x, cb1[1].y, r0.y       ; b = -r0.x
//   ret
constexpr std::size_t kSecondMad = 38; // token index of the second mad
std::vector<std::uint32_t> sprite_program() {
    return {
        0x00010050, 50,                                                         // vs_5_0, 50 tokens
        0x04000059, 0x00208E46, 0, 23,                                          // dcl_constantbuffer cb0[23]
        0x04000059, 0x00208E46, 1, 2,                                           // dcl_constantbuffer cb1[2]
        0x02000068, 3,                                                          // dcl_temps 3
        0x08000011, 0x00100042, 0, 0x00100E46, 2,    0x00208E46, 0,          0, // dp4 r0.z, r2, cb0[0]
        0x0A000032, 0x00100012, 1, 0x0010001A, 0,    0x0020800A, 1,          0, 0x0010002A, 0, // mad r1.x
        0x08000011, 0x00100022, 0, 0x00100E46, 2,    0x00208E46, 0,          1, // dp4 r0.y, r2, cb0[1]
        0x0B000032, 0x00100022, 1, 0x8010000A, 0x41, 0,          0x0020801A, 1, 1,          0x0010001A,
        0,          // mad r1.y
        0x0100003E, // ret
    };
}

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

std::vector<std::uint32_t> program_of(const std::vector<std::uint8_t>& blob) {
    std::uint32_t bytes = 0;
    std::memcpy(&bytes, blob.data() + 36 + 4, 4);
    std::vector<std::uint32_t> t(bytes / 4);
    std::memcpy(t.data(), blob.data() + 44, bytes);
    return t;
}

// A small interpreter of the shader model 5 instructions the rewritten
// program uses, over temporaries and constant buffers 0 and 1.
struct Machine {
    std::array<std::array<float, 4>, 8> r{};
    std::array<std::array<std::array<float, 4>, 24>, 2> cb{};

    struct Src {
        std::array<float, 4> v;
    };

    static std::uint32_t sel(std::uint32_t tok, int c) { return (tok >> (4 + 2 * c)) & 3; }

    // Reads the source operand at t[p]; advances p.
    std::array<float, 4> src(const std::vector<std::uint32_t>& t, std::size_t& p) {
        const std::uint32_t tok = t[p++];
        bool neg                = false;
        if (tok & 0x80000000u) neg = (t[p++] & 0x40) != 0;
        const std::uint32_t type = (tok >> 12) & 0xFF;
        std::array<float, 4> v{};
        if (type == 4) { // immediate
            const int n = (tok & 3) == 2 ? 4 : 1;
            for (int i = 0; i < n; ++i) v[i] = std::bit_cast<float>(t[p++]);
            if (n == 1) v = {v[0], v[0], v[0], v[0]};
        } else {
            std::array<float, 4> reg{};
            if (type == 0) {
                reg = r.at(t[p++]);
            } else {
                REQUIRE(type == 8);
                const std::uint32_t b = t[p++];
                reg                   = cb.at(b).at(t[p++]);
            }
            const std::uint32_t mode = (tok >> 2) & 3;
            for (int c = 0; c < 4; ++c) v[c] = reg[mode == 2 ? sel(tok, 0) : sel(tok, c)];
        }
        if (neg)
            for (auto& x : v) x = -x;
        return v;
    }

    void run(const std::vector<std::uint32_t>& t) {
        for (std::size_t i = 2; i < t[1];) {
            const std::uint32_t op  = t[i] & 0x7FF;
            const std::uint32_t len = (t[i] >> 24) & 0x7F;
            if (op == 0x3E) break;          // ret
            if (op >= 0x58 && op <= 0x6A) { // declarations
                i += len;
                continue;
            }
            std::size_t p           = i + 1;
            const std::uint32_t dtk = t[p++];
            const std::uint32_t dr  = t[p++];
            const std::uint32_t msk = (dtk >> 4) & 0xF;
            std::array<float, 4> a  = src(t, p), res{};
            std::array<float, 4> b{}, c{};
            if (op != 0x36 && op != 0x4B && op != 0x44) b = src(t, p);
            if (op == 0x32 || op == 0x37) c = src(t, p);
            REQUIRE(p == i + len);
            for (int k = 0; k < 4; ++k) {
                switch (op) {
                    case 0x36: res[k] = a[k]; break;                                                  // mov
                    case 0x00: res[k] = a[k] + b[k]; break;                                           // add
                    case 0x38: res[k] = a[k] * b[k]; break;                                           // mul
                    case 0x32: res[k] = a[k] * b[k] + c[k]; break;                                    // mad
                    case 0x34: res[k] = std::max(a[k], b[k]); break;                                  // max
                    case 0x0F: res[k] = a[0] * b[0] + a[1] * b[1]; break;                             // dp2
                    case 0x11: res[k] = a[0] * b[0] + a[1] * b[1] + a[2] * b[2] + a[3] * b[3]; break; // dp4
                    case 0x4B: res[k] = std::sqrt(a[k]); break;                                       // sqrt
                    case 0x44: res[k] = 1.0f / std::sqrt(a[k]); break;                                // rsq
                    case 0x31: res[k] = std::bit_cast<float>(a[k] < b[k] ? 0xFFFFFFFFu : 0u); break;  // lt
                    case 0x37: res[k] = std::bit_cast<std::uint32_t>(a[k]) != 0 ? b[k] : c[k]; break; // movc
                    default: FAIL("unexpected opcode " << op);
                }
            }
            // Scalar-result instructions replicate; masks pick components.
            for (int k = 0; k < 4; ++k)
                if (msk & (1u << k)) r.at(dr)[k] = res[k];
            i += len;
        }
    }
};

// Runs `program` with a sprite corner offset (a, b) and the world's up at
// view (ux, uy); returns r1.xy.
std::array<float, 2> run_sprite(const std::vector<std::uint32_t>& program, float a, float b, float ux,
                                float uy) {
    Machine m;
    m.r[2]        = {0.5f, -0.25f, 2.0f, 1.0f}; // the sprite centre
    m.cb[0][0]    = {1, 0, 0, 0};               // ViewProj rows as the dp4s read them
    m.cb[0][1]    = {0, 1, 0, 0};
    m.cb[0][4][1] = ux;
    m.cb[0][5][1] = uy;
    m.cb[1][0][0] = 2.0f; // P[0][0]
    m.cb[1][1][1] = 3.0f; // P[1][1]
    m.r[0]        = {-b, a, 0, 0};
    m.run(program);
    return {m.r[1][0], m.r[1][1]};
}

TEST_CASE("the sprite rewrite turns the corner and keeps the container valid") {
    const auto in  = container(sprite_program());
    int sites      = 0;
    const auto out = patch_upright_sprites(in.data(), in.size(), sites);
    REQUIRE(sites == 1);
    REQUIRE(out.size() > in.size());
    std::uint32_t total = 0;
    std::memcpy(&total, out.data() + 24, 4);
    CHECK(total == out.size());
    const DxbcChecksum sum = dxbc_checksum(out);
    CHECK(std::memcmp(sum.data(), out.data() + 4, 16) == 0);

    const auto t = program_of(out);
    CHECK(t[1] == t.size());
    CHECK(t[11] == 5u); // dcl_temps 3 + 2

    // The game's own result first: centre (0.5, -0.25) plus (a·2, b·3).
    const auto orig = run_sprite(sprite_program(), 0.25f, -0.5f, 0.0f, 1.0f);
    CHECK(orig[0] == doctest::Approx(0.5f + 0.5f));
    CHECK(orig[1] == doctest::Approx(-0.25f - 1.5f));

    // No roll: the same.
    const auto upright = run_sprite(t, 0.25f, -0.5f, 0.0f, 0.9f);
    CHECK(upright[0] == doctest::Approx(orig[0]).epsilon(kEps));
    CHECK(upright[1] == doctest::Approx(orig[1]).epsilon(kEps));

    // Rolled by 30 degrees (24 §14.3 geometry): a unit right offset becomes
    // (cos 30, -sin 30) in the view, times the projection.
    const float r     = std::numbers::pi_v<float> / 6.0f;
    const auto rolled = run_sprite(t, 1.0f, 0.0f, std::sin(r), std::cos(r));
    CHECK(rolled[0] == doctest::Approx(0.5f + 2.0f * std::cos(r)).epsilon(kEps));
    CHECK(rolled[1] == doctest::Approx(-0.25f - 3.0f * std::sin(r)).epsilon(kEps));
    const auto rolled_up = run_sprite(t, 0.0f, 1.0f, std::sin(r), std::cos(r));
    CHECK(rolled_up[0] == doctest::Approx(0.5f + 2.0f * std::sin(r)).epsilon(kEps));
    CHECK(rolled_up[1] == doctest::Approx(-0.25f + 3.0f * std::cos(r)).epsilon(kEps));

    // Straight down, and upside down near the vertical: the game's own.
    for (const auto& u : {std::array<float, 2>{0.0f, 0.0f}, std::array<float, 2>{0.0f, -0.05f}}) {
        const auto v = run_sprite(t, 0.25f, -0.5f, u[0], u[1]);
        CHECK(v[0] == doctest::Approx(orig[0]).epsilon(kEps));
        CHECK(v[1] == doctest::Approx(orig[1]).epsilon(kEps));
    }
    // Eased half way (w = (0.05, 0.05)).
    const auto half = run_sprite(t, 1.0f, 0.0f, 0.05f, 0.0f);
    CHECK(half[0] == doctest::Approx(0.5f + 2.0f * std::sqrt(0.5f)).epsilon(kEps));
    CHECK(half[1] == doctest::Approx(-0.25f - 3.0f * std::sqrt(0.5f)).epsilon(kEps));
}

TEST_CASE("the sprite rewrite refuses what is not the game's sprite form") {
    int sites = 0;

    std::vector<std::uint32_t> pixel = sprite_program();
    pixel[0]                         = 0x00000050; // ps_5_0
    const auto ps                    = container(pixel);
    CHECK(patch_upright_sprites(ps.data(), ps.size(), sites).empty());

    std::vector<std::uint8_t> bad_sum = container(sprite_program());
    bad_sum[5] ^= 1;
    CHECK(patch_upright_sprites(bad_sum.data(), bad_sum.size(), sites).empty());

    // b written between the two: the value read before the first would differ.
    std::vector<std::uint32_t> clobber = sprite_program();
    clobber[31]                        = 0x00100032; // the second dp4 writes r0.xy
    const auto c                       = container(clobber);
    CHECK(patch_upright_sprites(c.data(), c.size(), sites).empty());

    // The clip coordinate not from ViewProj.
    std::vector<std::uint32_t> other_clip = sprite_program();
    other_clip[19]                        = 7; // dp4 with cb0[7]
    const auto o                          = container(other_clip);
    CHECK(patch_upright_sprites(o.data(), o.size(), sites).empty());

    // cb0 too small to hold ViewMatrix.
    std::vector<std::uint32_t> small_cb0 = sprite_program();
    small_cb0[5]                         = 4;
    const auto s                         = container(small_cb0);
    CHECK(patch_upright_sprites(s.data(), s.size(), sites).empty());

    // Only one of the two corners.
    std::vector<std::uint32_t> one = sprite_program();
    one[kSecondMad + 7]            = 2; // the y mad reads cb2[1].y
    const auto x                   = container(one);
    CHECK(patch_upright_sprites(x.data(), x.size(), sites).empty());
    CHECK(sites == 0);
}

} // namespace
