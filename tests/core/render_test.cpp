// core/render: the DXBC checksum against fxc's own on the project's mirror
// shaders, and the position-dither fix on a synthetic pixel shader.

#include "core/render/dither_fix.h"
#include "core/render/dxbc.h"

#include <doctest/doctest.h>

#include <array>
#include <cstdint>
#include <cstring>
#include <span>
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
    int sites = -1;
    CHECK(patch_position_dither(kMirrorPS, sizeof(kMirrorPS), sites).empty());
    CHECK(sites == 0);
    const std::array<std::uint8_t, 8> junk = {1, 2, 3, 4, 5, 6, 7, 8};
    CHECK(patch_position_dither(junk.data(), junk.size(), sites).empty());
}

// 0.001f, the scale every dither site multiplies the pixel position by.
constexpr std::uint32_t kPoint001 = 0x3A83126F;

// A ps_5_0 program: dcl cb2[10]; mul r0.xy, v0.xyxx, l(0.001, 0.001, 0, 0);
// mad r1.xyzw, v0.xxxy, l(0, 0, 0.001, 0.001), -v1.xxxx; ret.
std::vector<std::uint32_t> dither_program() {
    return {
        0x00000050, 30,                                           // ps_5_0, 30 tokens
        0x04000059, 0x00208E46, 2,         10,                    // dcl_constantbuffer cb2[10]
        0x0A000038, 0x00100032, 0,         0x00101046, 0,         // mul r0.xy, v0.xyxx,
        0x00004002, kPoint001,  kPoint001, 0,          0,         //     l(0.001, 0.001, 0, 0)
        0x0D000032, 0x001000F2, 1,         0x00101406, 0,         // mad r1.xyzw, v0.xxxy,
        0x00004002, 0,          0,         kPoint001,  kPoint001, //     l(0, 0, 0.001, 0.001),
        0x80101006, 0x00000041, 1,                                //     -v1.xxxx
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

TEST_CASE("the dither fix rewrites one site and keeps the container valid") {
    const std::vector<std::uint8_t> in  = container(dither_program());
    int sites                           = 0;
    const std::vector<std::uint8_t> out = patch_position_dither(in.data(), in.size(), sites);
    REQUIRE(sites == 1);
    REQUIRE(out.size() == in.size() + 32); // 8 tokens more per site
    CHECK(u32_at(out, 24) == out.size());
    CHECK(u32_at(out, 36 + 4) == 30 * 4 + 32);
    check_stored_checksum(out);

    std::vector<std::uint32_t> t((out.size() - 44) / 4);
    std::memcpy(t.data(), out.data() + 44, t.size() * 4);
    CHECK(t[1] == 38u);         // program length
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
    CHECK(t[37] == 0x0100003Eu); // ret
}

TEST_CASE("the dither fix refuses what it does not understand") {
    int sites                         = 0;
    std::vector<std::uint8_t> bad_sum = container(dither_program());
    bad_sum[4] ^= 1;
    CHECK(patch_position_dither(bad_sum.data(), bad_sum.size(), sites).empty());

    std::vector<std::uint32_t> vertex  = dither_program();
    vertex[0]                          = 0x00010050; // vs_5_0
    const std::vector<std::uint8_t> vs = container(vertex);
    CHECK(patch_position_dither(vs.data(), vs.size(), sites).empty());

    std::vector<std::uint32_t> no_cb2     = dither_program();
    no_cb2[4]                             = 1; // cb1
    const std::vector<std::uint8_t> other = container(no_cb2);
    CHECK(patch_position_dither(other.data(), other.size(), sites).empty());
    CHECK(sites == 0);
}

} // namespace
