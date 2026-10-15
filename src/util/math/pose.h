#pragma once

#include "util/math/vec.h"

#include <cmath>

namespace omvr::util {

struct Pose {
    Quat orientation{};
    Vec3 position{};
};

// Row-major 3x4 of a pose (the OpenVR/OpenXR axis convention: +Y up, -Z forward).
inline void pose_matrix(const Pose& p, float (&m)[3][4]) noexcept {
    const auto& q = p.orientation;
    m[0][0] = 1 - 2 * (q.y * q.y + q.z * q.z); m[0][1] = 2 * (q.x * q.y - q.z * q.w); m[0][2] = 2 * (q.x * q.z + q.y * q.w);
    m[1][0] = 2 * (q.x * q.y + q.z * q.w); m[1][1] = 1 - 2 * (q.x * q.x + q.z * q.z); m[1][2] = 2 * (q.y * q.z - q.x * q.w);
    m[2][0] = 2 * (q.x * q.z - q.y * q.w); m[2][1] = 2 * (q.y * q.z + q.x * q.w); m[2][2] = 1 - 2 * (q.x * q.x + q.y * q.y);
    m[0][3] = p.position.x;
    m[1][3] = p.position.y;
    m[2][3] = p.position.z;
}

// The heading of `q` about +Y.
inline float yaw_of(const Quat& q) noexcept {
    return std::atan2(2 * (q.w * q.y + q.x * q.z), 1 - 2 * (q.x * q.x + q.y * q.y));
}

} // namespace omvr::util
