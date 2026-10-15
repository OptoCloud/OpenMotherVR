#pragma once

#include <cmath>

namespace omvr::util {

struct Vec2 { float x = 0, y = 0; };
struct Vec3 { float x = 0, y = 0, z = 0; };
struct Quat { float x = 0, y = 0, z = 0, w = 1; };

// `v` rotated by the unit quaternion `q`.
inline Vec3 rotate(const Quat& q, const Vec3& v) noexcept {
    // v + 2w(q x v) + 2 q x (q x v)
    const Vec3 u{q.x, q.y, q.z};
    const Vec3 t{2 * (u.y * v.z - u.z * v.y), 2 * (u.z * v.x - u.x * v.z), 2 * (u.x * v.y - u.y * v.x)};
    return {v.x + q.w * t.x + (u.y * t.z - u.z * t.y),
            v.y + q.w * t.y + (u.z * t.x - u.x * t.z),
            v.z + q.w * t.z + (u.x * t.y - u.y * t.x)};
}

inline Quat conjugate(const Quat& q) noexcept { return {-q.x, -q.y, -q.z, q.w}; }

inline Vec3 sub(const Vec3& a, const Vec3& b) noexcept { return {a.x - b.x, a.y - b.y, a.z - b.z}; }

inline float length(const Vec3& v) noexcept { return std::sqrt(v.x * v.x + v.y * v.y + v.z * v.z); }

inline float length(float x, float y, float z) noexcept { return std::sqrt(x * x + y * y + z * z); }

} // namespace omvr::util
