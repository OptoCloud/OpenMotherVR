#pragma once

#include "util/math/matrix.h"

namespace omvr::core::vr {

// The head pose conversion both original backends apply: negate the X and Y
// basis columns, negate the translation and scale it.
// From: OpenVR slot 5 (FUN_100070e0), the multiply by diag(-1,-1,1,-s).
inline util::Matrix34 to_game_pose(const float (&r)[3][4], float s) noexcept {
    util::Matrix34 o{};
    for (int i = 0; i < 3; ++i) {
        o.m[i][0] = -r[i][0];
        o.m[i][1] = -r[i][1];
        o.m[i][2] =  r[i][2];
        o.m[i][3] = -s * r[i][3];
    }
    return o;
}

// The velocity conversion that goes with to_game_pose.
inline void to_game_velocity(const float (&v)[3], float s, float (&out)[3]) noexcept {
    for (int i = 0; i < 3; ++i) out[i] = -s * v[i];
}

} // namespace omvr::core::vr
