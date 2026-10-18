// OpenMotherVR-Provenance: clean
#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

// Pixel shader fixes applied when the game creates its shaders (08 §1).
// Pure; runs on whichever thread creates the shader.
namespace omvr::core::render {

// Many of the game's pixel shaders (several hundred per level) add an
// anti-banding dither computed from the absolute pixel position p
// (08 §1.1), in this layout or one of several others (r7: the written
// components and the temporaries vary):
//
//   mul rA.xy, vPos.xyxx, l(0.001, 0.001, 0, 0)          ; rA = 0.001 p
//   mad rB.zw, vPos.xxxy, l(0, 0, 0.001, 0.001), -vT.xxxx ; rB = 0.001 p - t
//   dp2 d, rB.zwzz, rA.xyxx                               ; d = (0.001 p - t).(0.001 p)
//   exp / frc ... added to a lighting factor, min(..., 1)
//
// On a side-by-side stereo target the right eye's x is large, the exp
// overflows, frc returns NaN and min() turns the lighting factor into 1:
// surfaces light up in the right eye, more so the wider the target. The fix
// replaces p with n, a position in [0, 1) that does not grow with the
// target, and keeps the 0.001 factor:
//
//   mul rA.xy, vPos.xyxx, cb2[39].zwzz                    ; rA = n = p / size
//   mad rB.zw, rA.xxxy, l(0, 0, 0.001, 0.001), -vT.xxxx   ; rB = 0.001 n - t
//   mul rA.xy, rA.xyzw, l(0.001, 0.001, 0, 0)             ; rA = 0.001 n
//
// so d = (0.001 n - t).(0.001 n), about -0.001 t (n.x + n.y): small,
// independent of the resolution, and the same for both eyes. cb2[39] =
// (W, H, 1/W, 1/H) is the render target's size (08 §1.3); cb2 is declared
// at least 40 registers. A shader that declares no cb2 gets no new buffer
// and uses n = frc(p / 1024) instead (08 §1.4 r7):
//
//   mul rA.xy, vPos.xyxx, l(1/1024, 1/1024, 0, 0)
//   frc rA.xy, rA.xyzw
//
// The program grows (8 tokens per site with cb2, 15 without); the
// container's sizes, the later chunk offsets and the checksum are
// rewritten. Only rA's and rB's written components change, as the
// original pair writes them.
struct DitherPatch {
    // The patched container, or empty when the shader is left unchanged.
    std::vector<std::uint8_t> blob;
    // The dither sites rewritten (0 when unchanged).
    int sites = 0;
    // The shader holds two consecutive 0.001 immediates (08 §1.4's
    // pre-filter): with an empty blob, a shader that may hold the dither in
    // a form left unchanged (11 §2.2 r8).
    bool has_constants = false;
};

// The shader with every dither site rewritten, or an empty blob when it has
// no site or anything about it is unexpected (then the original is used).
DitherPatch patch_position_dither(const void* blob, std::size_t size) noexcept;

} // namespace omvr::core::render
