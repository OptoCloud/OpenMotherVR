#pragma once

#include "util/math/matrix.h"

namespace omvr::core::vr {

// The head pose conversion both original backends apply: negate the X and Y
// basis columns, negate the translation and scale it by the world scale `s`
// (03 §1).
// From: OpenVR slot 5 (FUN_100070e0), the multiply by diag(-1,-1,1,-s).
inline util::Matrix34 to_game_pose(const float (&r)[3][4], float s) noexcept {
    constexpr int kRows = 3;
    util::Matrix34 o{};
    for (int i = 0; i < kRows; ++i) {
        o.m[i][0] = -r[i][0];
        o.m[i][1] = -r[i][1];
        o.m[i][2] =  r[i][2];
        o.m[i][3] = -s * r[i][3];
    }
    return o;
}

// The velocity conversion that goes with to_game_pose: a position p maps to
// -s * p, so a velocity v to -s * v.
inline void to_game_velocity(const float (&v)[3], float s, float (&out)[3]) noexcept {
    constexpr int kAxes = 3;
    for (int i = 0; i < kAxes; ++i) out[i] = -s * v[i];
}

} // namespace omvr::core::vr
