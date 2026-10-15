// UI quad placement (05 §4). Analysis: dumps/06c_detours_and_patches.md.

#include "core/stereo/ui_placement.h"

#include "core/stereo/tuning.h"
#include "util/math/matrix.h"
#include "util/math/vec.h"

#include <cmath>
#include <cstddef>

namespace omvr::core::stereo {

namespace {

using util::cross;
using util::scaled;
using util::Vec3;

// Rows of a quad matrix: its right and up axes (scaled to the quad's size),
// its forward axis and its position.
constexpr std::size_t kRightRow    = 0;
constexpr std::size_t kUpRow       = 1;
constexpr std::size_t kForwardRow  = 2;
constexpr std::size_t kPositionRow = 3;

// Columns of the view matrix: the camera's right, up and forward axes in
// world space (rows 0 to 2).
constexpr std::size_t kViewUpColumn      = 1;
constexpr std::size_t kViewForwardColumn = 2;

Matrix44 identity_quad() noexcept {
    Matrix44 m{};
    util::identity(m.data());
    return m;
}

// Writes x, y, z of `row`; the fourth column keeps the identity's value.
void set_row(Matrix44& m, std::size_t row, const Vec3& v) noexcept {
    m[row * util::kMatrix4Rows + 0] = v.x;
    m[row * util::kMatrix4Rows + 1] = v.y;
    m[row * util::kMatrix4Rows + 2] = v.z;
}

Vec3 view_axis(const Matrix44& v, std::size_t column) noexcept {
    return {util::element4(v.data(), 0, column), util::element4(v.data(), 1, column),
            util::element4(v.data(), 2, column)};
}

// The camera's forward axis flattened onto the ground plane (x, z), as a
// unit vector; +Z when the camera looks straight up or down.
Vec3 level_forward(const Matrix44& v) noexcept {
    float fx        = util::element4(v.data(), 0, kViewForwardColumn);
    float fz        = util::element4(v.data(), 2, kViewForwardColumn);
    const float len = std::sqrt(fx * fx + fz * fz);
    if (len > kMinForwardLength) {
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
    Matrix44 out = identity_quad();
    if (!view) return out;
    const Vec3 fwd = level_forward(*view);
    // Right = up x forward (left-handed), up scaled to the aspect.
    set_row(out, kRightRow, {fwd.z, 0.0f, -fwd.x});
    set_row(out, kUpRow, {0.0f, inv_aspect, 0.0f});
    set_row(out, kForwardRow, fwd);
    const Vec3 cam = util::camera_position(view->data());
    set_row(out, kPositionRow,
            {cam.x + fwd.x * kMenuScreenDistance, cam.y, cam.z + fwd.z * kMenuScreenDistance});
    return out;
}

Matrix44 hud_panel_quad(const std::optional<Matrix44>& view, float inv_aspect) noexcept {
    Matrix44 out = identity_quad();
    if (!view) return out;
    const Vec3 up  = view_axis(*view, kViewUpColumn);
    const Vec3 fwd = view_axis(*view, kViewForwardColumn);
    set_row(out, kRightRow, scaled(cross(up, fwd), kHudPanelWidth));
    set_row(out, kUpRow,
            {up.x * kHudPanelWidth * inv_aspect, up.y * kHudPanelWidth * inv_aspect,
             up.z * kHudPanelWidth * inv_aspect});
    set_row(out, kForwardRow, fwd);
    const Vec3 cam = util::camera_position(view->data());
    set_row(out, kPositionRow, {fwd.x + cam.x, fwd.y + cam.y, fwd.z + cam.z});
    return out;
}

} // namespace omvr::core::stereo
