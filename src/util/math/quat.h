// OpenMotherVR-Provenance: clean
#pragma once

#include "util/math/matrix.h"
#include "util/math/vec.h"

#include <algorithm>
#include <cmath>

// Vector and quaternion helpers beyond vec.h: products, the quaternion of a
// rotation matrix, interpolation. Plain maths, any axis convention: the
// caller's matrices and quaternions only have to share one.
namespace omvr::util {

inline Vec3 add(const Vec3& a, const Vec3& b) noexcept { return {a.x + b.x, a.y + b.y, a.z + b.z}; }

inline float dot(const Vec3& a, const Vec3& b) noexcept { return a.x * b.x + a.y * b.y + a.z * b.z; }

// a + (b - a) * t.
inline Vec3 lerp(const Vec3& a, const Vec3& b, float t) noexcept {
    return {a.x + (b.x - a.x) * t, a.y + (b.y - a.y) * t, a.z + (b.z - a.z) * t};
}

// The Hamilton product a * b: rotating by the result rotates by b, then a.
inline Quat multiply(const Quat& a, const Quat& b) noexcept {
    return {a.w * b.x + a.x * b.w + a.y * b.z - a.z * b.y, a.w * b.y - a.x * b.z + a.y * b.w + a.z * b.x,
            a.w * b.z + a.x * b.y - a.y * b.x + a.z * b.w, a.w * b.w - a.x * b.x - a.y * b.y - a.z * b.z};
}

inline float dot(const Quat& a, const Quat& b) noexcept {
    return a.x * b.x + a.y * b.y + a.z * b.z + a.w * b.w;
}

// `q` scaled to unit length; the identity when `q` has no length.
inline Quat normalized(const Quat& q) noexcept {
    const float n = std::sqrt(dot(q, q));
    if (!(n > 0.0f)) return {};
    return {q.x / n, q.y / n, q.z / n, q.w / n};
}

// The unit quaternion of the rotation in the 3x3 part of `m` (columns are
// the rotated axes: v' = R v). Shepperd's method: the largest of the four
// candidates is computed from the diagonal, so it is never near zero.
inline Quat quat_from_rotation(const Matrix34& r) noexcept {
    const auto& m     = r.m;
    const float trace = m[0][0] + m[1][1] + m[2][2];
    Quat q;
    if (trace > 0.0f) {
        const float s = std::sqrt(trace + 1.0f) * 2;
        q             = {(m[2][1] - m[1][2]) / s, (m[0][2] - m[2][0]) / s, (m[1][0] - m[0][1]) / s, s / 4};
    } else if (m[0][0] > m[1][1] && m[0][0] > m[2][2]) {
        const float s = std::sqrt(1.0f + m[0][0] - m[1][1] - m[2][2]) * 2;
        q             = {s / 4, (m[0][1] + m[1][0]) / s, (m[0][2] + m[2][0]) / s, (m[2][1] - m[1][2]) / s};
    } else if (m[1][1] > m[2][2]) {
        const float s = std::sqrt(1.0f + m[1][1] - m[0][0] - m[2][2]) * 2;
        q             = {(m[0][1] + m[1][0]) / s, s / 4, (m[1][2] + m[2][1]) / s, (m[0][2] - m[2][0]) / s};
    } else {
        const float s = std::sqrt(1.0f + m[2][2] - m[0][0] - m[1][1]) * 2;
        q             = {(m[0][2] + m[2][0]) / s, (m[1][2] + m[2][1]) / s, s / 4, (m[1][0] - m[0][1]) / s};
    }
    return normalized(q);
}

// Normalised linear interpolation from `a` to `b` along the shorter arc.
inline Quat nlerp(const Quat& a, const Quat& b, float t) noexcept {
    const float sign = dot(a, b) < 0.0f ? -1.0f : 1.0f;
    return normalized({a.x + (sign * b.x - a.x) * t, a.y + (sign * b.y - a.y) * t,
                       a.z + (sign * b.z - a.z) * t, a.w + (sign * b.w - a.w) * t});
}

// The angle in radians of the rotation from unit `a` to unit `b`, 0..pi.
inline float angle_between(const Quat& a, const Quat& b) noexcept {
    const float c = std::min(1.0f, std::fabs(dot(a, b)));
    return 2 * std::acos(c);
}

// The translation column of a 3x4 pose.
inline Vec3 translation_of(const Matrix34& p) noexcept { return {p.m[0][3], p.m[1][3], p.m[2][3]}; }

// R^T v for the 3x3 part of `r`: a world vector in the pose's own axes.
inline Vec3 rotate_inverse(const Matrix34& r, const Vec3& v) noexcept {
    const auto& m = r.m;
    return {m[0][0] * v.x + m[1][0] * v.y + m[2][0] * v.z, m[0][1] * v.x + m[1][1] * v.y + m[2][1] * v.z,
            m[0][2] * v.x + m[1][2] * v.y + m[2][2] * v.z};
}

// The 3x3 product A^T B of two poses' rotations, as a 3x4 with no
// translation: B's rotation in A's axes.
inline Matrix34 relative_rotation(const Matrix34& a, const Matrix34& b) noexcept {
    Matrix34 out{};
    for (int r = 0; r < 3; ++r) {
        for (int c = 0; c < 3; ++c)
            out.m[r][c] = a.m[0][r] * b.m[0][c] + a.m[1][r] * b.m[1][c] + a.m[2][r] * b.m[2][c];
    }
    return out;
}

} // namespace omvr::util
