// OpenMotherVR-Provenance: clean
#pragma once

#include "core/vr/types.h"

#include <cstdint>

// What each eye sees and what the game renders for it: the per-eye
// tangents, the symmetric FOV both eyes render with, the crop that maps it
// back to each eye, and the render size.
namespace omvr::core::vr {

// A projection as OpenVR's GetProjectionRaw reports it: the tangents of the
// view's extents, left and top negative, bottom positive (03 §3.1).
struct RawProjection {
    float left, right, top, bottom;
};

// The symmetric FOV both eyes render with, as tangents (03 §3.2).
struct RenderTangents {
    float vertical, horizontal;
};

// One eye's tangents from its raw projection. In image terms (v = 0 at the
// top) the raw bottom is the upward extent (03 §3.1).
FovTangents tangents_from_raw(const RawProjection& p) noexcept;

// One eye's tangents from its angles (09 §4.1).
FovTangents tangents_from_angles(const FovAngles& a) noexcept;

// The largest extent of either eye, vertically and horizontally (03 §3.2).
RenderTangents render_tangents(const FovTangents& left, const FovTangents& right) noexcept;

// The symmetric FOV as tangents of every side, for HeadsetDesc::max_eye_fov.
FovTangents symmetric_tangents(RenderTangents t) noexcept;

// The symmetric FOV as angles, as a projection layer declares it (09 §5.2).
FovAngles symmetric_angles(RenderTangents t) noexcept;

// The part of eye `eye`'s half of the side-by-side texture that holds the
// eye's real FOV: each eye renders the symmetric frustum, and the crop maps
// it back (03 §3.3, the SteamVR Unity plugin formula).
TextureBounds crop_bounds(int eye, const RawProjection& p, RenderTangents t) noexcept;

// The runtime's recommended per-eye size times `pixel_scale` (03 §4.1),
// truncated to whole pixels.
TextureSize scaled_size(std::uint32_t width, std::uint32_t height, float pixel_scale) noexcept;

} // namespace omvr::core::vr
