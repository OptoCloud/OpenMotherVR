#pragma once

#include "core/stereo/types.h"
#include "core/vr/types.h"

// The size and eye layout of the side-by-side stereo render target (03 §4).
// Pure.
namespace omvr::core::stereo {

// The configured pixel scale, capped so the side-by-side target of the
// runtime's per-eye sizes at scale 1 stays within kMaxTextureSize. PixelScale
// multiplies the runtime's recommended size, which already includes
// SteamVR's own resolution setting (03 §10).
float capped_pixel_scale(float pixel_scale, vr::TextureSize left, vr::TextureSize right) noexcept;

// Both eyes side by side: their widths summed, and the taller eye's height
// so neither eye's image is cut off. (The original took the height of the
// wider eye.)
PixelSize side_by_side(vr::TextureSize left, vr::TextureSize right) noexcept;

// Eye 0 the left half, eye 1 the right half.
EyeViewports eye_viewports(PixelSize target) noexcept;

// Whether the eye layout follows a newly computed size now (03 §4.3): before
// any texture was configured, or when the texture in use already has that
// size. Otherwise the layout stays with the texture in use until the game
// makes one of the new size (it then configures rendering again).
bool layout_follows(PixelSize size, PixelSize configured) noexcept;

} // namespace omvr::core::stereo
