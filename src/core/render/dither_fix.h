#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

// Pixel shader fixes applied when the game creates its shaders.
namespace omvr::core::render {

// Many of the game's pixel shaders (several hundred per level) add an
// anti-banding dither computed from the absolute pixel position p:
//
//   mul rA.xy, vPos.xyxx, l(0.001, 0.001, 0, 0)          ; rA = 0.001 p
//   mad rB.zw, vPos.xxxy, l(0, 0, 0.001, 0.001), -vT.xxxx ; rB = 0.001 p - t
//   dp2 d, rB.zwzz, rA.xyxx                               ; d = (0.001 p - t).(0.001 p)
//   exp / frc ... added to a lighting factor, min(..., 1)
//
// On a side-by-side stereo target the right eye's x is large, the exp
// overflows, frc returns NaN and min() turns the lighting factor into 1:
// surfaces light up in the right eye, more so the wider the target. The fix
// replaces p with n = p / size, the position
// relative to the render target, which the game passes to its shaders as
// cb2[39] = (W, H, 1/W, 1/H), and keeps the 0.001 factor:
//
//   mul rA.xy, vPos.xyxx, cb2[39].zwzz                    ; rA = n
//   mad rB.zw, rA.xxxy, l(0, 0, 0.001, 0.001), -vT.xxxx   ; rB = 0.001 n - t
//   mul rA.xy, rA.xyzw, l(0.001, 0.001, 0, 0)             ; rA = 0.001 n
//
// so d = (0.001 n - t).(0.001 n), about -0.001 t (n.x + n.y): small,
// independent of the resolution, and the same for both eyes. The first mul
// gets two tokens shorter and the inserted one adds ten, so the program
// chunk grows by 32 bytes per site (the container's sizes, the later chunk
// offsets and the checksum are rewritten). cb2 is declared at least 40
// registers.
//
// Returns the patched blob, or an empty vector when the shader has no such
// site or anything about it is unexpected (then the original is used).
std::vector<std::uint8_t> patch_position_dither(const void* blob, std::size_t size, int& sites) noexcept;

} // namespace omvr::core::render
