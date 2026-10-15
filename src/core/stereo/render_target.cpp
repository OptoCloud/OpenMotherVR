// From: FUN_100202a0 (the GetRenderTargetSize hook) and the device slot 16.
// Analysis: dumps/06a_stereo_interface.md.

#include "core/stereo/render_target.h"

#include "core/stereo/tuning.h"

#include <algorithm>

namespace omvr::core::stereo {

float capped_pixel_scale(float pixel_scale, vr::TextureSize left, vr::TextureSize right) noexcept {
    if (left.w + right.w <= 0 || left.h <= 0 || right.h <= 0) return pixel_scale;
    const float max_scale = std::min(kMaxTextureSize / static_cast<float>(left.w + right.w),
                                     kMaxTextureSize / static_cast<float>(std::max(left.h, right.h)));
    return std::min(pixel_scale, max_scale);
}

PixelSize side_by_side(vr::TextureSize left, vr::TextureSize right) noexcept {
    return {left.w + right.w, std::max(left.h, right.h)};
}

EyeViewports eye_viewports(PixelSize target) noexcept {
    const std::int32_t half = target.width / 2;
    return {{{0, 0, half, target.height}, {half, 0, half, target.height}}};
}

bool layout_follows(PixelSize size, PixelSize configured) noexcept {
    const bool texture_configured = configured.width != 0;
    return !texture_configured || size == configured;
}

} // namespace omvr::core::stereo
