// UI quad placement (05 §4). Analysis: dumps/06c_detours_and_patches.md.

#include "core/stereo/ui_placement.h"

#include "core/stereo/tuning.h"
#include "util/math/matrix.h"
#include "util/math/vec.h"

#include <cmath>

namespace omvr::core::stereo {

namespace {

using util::column3;
using util::cross;
using util::identity44;
using util::kForwardAxis;
using util::kRightAxis;
using util::kTranslationRow;
using util::kUpAxis;
using util::scaled;
using util::set_row3;
using util::Vec3;

// The camera's forward axis flattened onto the ground plane (x, z), as a
// unit vector; +Z when the camera looks straight up or down.
Vec3 level_forward(const Matrix44& v) noexcept {
    float fx        = util::element4(v, 0, kForwardAxis);
    float fz        = util::element4(v, 2, kForwardAxis);
    const float len = std::sqrt(fx * fx + fz * fz);
    if (len > kMinViewForwardLength) {
        fx /= len;
        fz /= len;
    } else {
        fx = 0.0f;
        fz = 1.0f;
    }
    return {fx, 0.0f, fz};
}

} // namespace

Matrix44 virtual_screen_quad(const std::optional<Matrix44>& view, float inv_aspect) noexcept {
    Matrix44 out = identity44();
    if (!view) return out;
    const Vec3 fwd = level_forward(*view);
    // Right = up x forward (left-handed), up scaled to the aspect.
    set_row3(out, kRightAxis, {fwd.z, 0.0f, -fwd.x});
    set_row3(out, kUpAxis, {0.0f, inv_aspect, 0.0f});
    set_row3(out, kForwardAxis, fwd);
    const Vec3 cam = util::camera_position(*view);
    set_row3(out, kTranslationRow,
             {cam.x + fwd.x * kMenuScreenDistance, cam.y, cam.z + fwd.z * kMenuScreenDistance});
    return out;
}

Matrix44 hud_panel_quad(const std::optional<Matrix44>& view, float inv_aspect) noexcept {
    Matrix44 out = identity44();
    if (!view) return out;
    const Vec3 up  = column3(*view, kUpAxis);
    const Vec3 fwd = column3(*view, kForwardAxis);
    set_row3(out, kRightAxis, scaled(cross(up, fwd), kHudPanelWidth));
    set_row3(out, kUpAxis,
             {up.x * kHudPanelWidth * inv_aspect, up.y * kHudPanelWidth * inv_aspect,
              up.z * kHudPanelWidth * inv_aspect});
    set_row3(out, kForwardAxis, fwd);
    const Vec3 cam = util::camera_position(*view);
    set_row3(out, kTranslationRow, {fwd.x + cam.x, fwd.y + cam.y, fwd.z + cam.z});
    return out;
}

} // namespace omvr::core::stereo
