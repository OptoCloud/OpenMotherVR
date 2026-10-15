#pragma once

#include "util/math/vec.h"

namespace omvr::util {

// Row-major 3x4, m[r][3] = translation.
struct Matrix34 {
    float m[3][4];
};

// The camera position of a rigid world-to-view matrix `v` (4x4, row vectors):
// minus the translation rotated back.
inline Vec3 camera_position(const float* v) noexcept {
    return {-(v[12] * v[0] + v[13] * v[1] + v[14] * v[2]),
            -(v[12] * v[4] + v[13] * v[5] + v[14] * v[6]),
            -(v[12] * v[8] + v[13] * v[9] + v[14] * v[10])};
}

// Sets the 4x4 `m` to identity.
inline void identity(float* m) noexcept {
    for (int i = 0; i < 16; ++i) m[i] = (i % 5 == 0) ? 1.0f : 0.0f;
}

} // namespace omvr::util
