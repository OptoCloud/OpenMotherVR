// UI quad placement. Analysis: dumps/06c_detours_and_patches.md.

#include "core/stereo/ui_placement.h"

#include "game/stereo/pov.h" // R: stereo (A3): the view matrix handed in as a value
#include "util/math/matrix.h"

#include <cmath>

namespace omvr::core::stereo {

using game::stereo::view_matrix;
using util::camera_position;
using util::identity;
using Vec3 = util::Vec3;

float* place_virtual_screen(float* out, float, float inv_aspect, float) noexcept {
    const float* v = view_matrix();
    identity(out);
    if (!v) return out;
    float fx = v[2], fz = v[10];
    const float len = std::sqrt(fx * fx + fz * fz);
    if (len > 1e-6f) {
        fx /= len;
        fz /= len;
    } else {
        fx = 0.0f;
        fz = 1.0f;
    }
    // Rows: right = up x forward (left-handed), up scaled to the aspect,
    // forward, position.
    out[0] = fz;   out[1] = 0.0f;       out[2]  = -fx;
    out[4] = 0.0f; out[5] = inv_aspect; out[6]  = 0.0f;
    out[8] = fx;   out[9] = 0.0f;       out[10] = fz;
    const Vec3 cam = camera_position(v);
    out[12] = cam.x + fx * 1.5f;
    out[13] = cam.y;
    out[14] = cam.z + fz * 1.5f;
    return out;
}

// The HUD in 3D: a panel 0.55 m wide, 1 m in front of the eyes, locked to
// the head. From: FUN_1001ffd0.
float* place_hud_panel(float* out, float, float inv_aspect, float) noexcept {
    const float* v = view_matrix();
    identity(out);
    if (!v) return out;
    constexpr float kWidth = 0.55f;
    const Vec3 up{v[1], v[5], v[9]};
    const Vec3 fwd{v[2], v[6], v[10]};
    out[0] = kWidth * (up.y * fwd.z - up.z * fwd.y);
    out[1] = kWidth * (up.z * fwd.x - up.x * fwd.z);
    out[2] = kWidth * (up.x * fwd.y - up.y * fwd.x);
    out[4] = up.x * kWidth * inv_aspect;
    out[5] = up.y * kWidth * inv_aspect;
    out[6] = up.z * kWidth * inv_aspect;
    out[8] = fwd.x;
    out[9] = fwd.y;
    out[10] = fwd.z;
    const Vec3 cam = camera_position(v);
    out[12] = fwd.x + cam.x;
    out[13] = fwd.y + cam.y;
    out[14] = fwd.z + cam.z;
    return out;
}

} // namespace omvr::core::stereo
