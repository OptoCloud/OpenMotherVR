// OpenMotherVR-Provenance: clean
#include "core/vr/fov.h"

#include <algorithm>
#include <cmath>

namespace omvr::core::vr {

FovTangents tangents_from_raw(const RawProjection& p) noexcept {
    return {p.bottom, -p.top, -p.left, p.right};
}

FovTangents tangents_from_angles(const FovAngles& a) noexcept {
    return {std::tan(a.up), std::tan(-a.down), std::tan(-a.left), std::tan(a.right)};
}

RenderTangents render_tangents(const FovTangents& left, const FovTangents& right) noexcept {
    return {std::max({left.up, left.down, right.up, right.down}),
            std::max({left.left, left.right, right.left, right.right})};
}

FovTangents symmetric_tangents(RenderTangents t) noexcept {
    return {t.vertical, t.vertical, t.horizontal, t.horizontal};
}

FovAngles symmetric_angles(RenderTangents t) noexcept {
    const float h = std::atan(t.horizontal);
    const float v = std::atan(t.vertical);
    return {-h, h, v, -v};
}

TextureBounds crop_bounds(int eye, const RawProjection& p, RenderTangents t) noexcept {
    // The frustum's centre maps to the middle of the eye's image; each eye
    // owns half of the texture's width.
    constexpr float kHalf = 0.5f;
    const float eye_u     = eye == 0 ? 0.0f : kHalf;
    return {(kHalf + kHalf * p.left / t.horizontal) * kHalf + eye_u, kHalf - kHalf * p.bottom / t.vertical,
            (kHalf + kHalf * p.right / t.horizontal) * kHalf + eye_u, kHalf - kHalf * p.top / t.vertical};
}

TextureSize scaled_size(std::uint32_t width, std::uint32_t height, float pixel_scale) noexcept {
    return {static_cast<std::int32_t>(static_cast<float>(width) * pixel_scale),
            static_cast<std::int32_t>(static_cast<float>(height) * pixel_scale)};
}

} // namespace omvr::core::vr
