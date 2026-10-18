// OpenMotherVR-Provenance: clean
#pragma once

#include "util/math/vec.h"

#include <cstddef>
#include <cstdint>
#include <vector>

// Upright sprites in stereo (24 §14). The game's camera-facing sprites add
// their corner offset (a, b) in the drawn eye's view plane, along the
// view's x and y axes, so they roll with the head. The fix turns (a, b) in
// that plane so that the sprite's up axis is the world's up as the eye sees
// it. Pure; the shader rewrite runs on whichever thread creates the shader.
namespace omvr::core::render {

// The sprite's up axis in view x and y for a view in which the world's up
// has the view coordinates (ux, uy) (24 §14.3 items 1 to 4): (ux, uy)
// normalised, eased back to (0, 1) within kUprightEaseLength of the
// vertical, and (0, 1) where it has no direction. Unit length.
util::Vec2 sprite_up_in_view(float ux, float uy) noexcept;

// The corner offset (a, b) turned so that the view's y axis becomes `up`
// (24 §14.3 item 5): (a·up.y + b·up.x, b·up.y - a·up.x).
util::Vec2 upright_offset(util::Vec2 offset, util::Vec2 up) noexcept;

// The axes in world space of a sprite drawn by a camera with the world-space
// axes `right`, `up` and `forward` (orthonormal), the world's up being +Y:
// what the rewritten shaders draw, stated in world space.
struct SpriteAxes {
    util::Vec3 right;
    util::Vec3 up;
    util::Vec3 forward;
};
SpriteAxes upright_sprite_axes(const util::Vec3& right, const util::Vec3& up,
                               const util::Vec3& forward) noexcept;

// The vertex shader `blob` with its sprite corner rewritten (24 §14.2,
// §14.3), or an empty vector when it is not a sprite shader of that form or
// anything about it is unexpected (then the original is used). The two
// `mad`s that add a·P[0][0] and b·P[1][1] then read (a', b'), computed
// right before the first of them into two new temporaries from cb0[4].y and
// cb0[5].y, the world's up in the drawn view.
std::vector<std::uint8_t> patch_upright_sprites(const void* blob, std::size_t size, int& sites) noexcept;

} // namespace omvr::core::render
