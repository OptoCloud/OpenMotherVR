// OpenMotherVR-Provenance: clean
#pragma once

#include "util/math/finite.h"
#include "util/math/vec.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <span>

namespace omvr::util {

// Row-major 3x4, m[r][3] = translation.
struct Matrix34 {
    float m[3][4];
};

// No value of the 3x4 is infinite or NaN.
inline bool all_finite(const Matrix34& p) noexcept {
    return std::ranges::all_of(p.m, [](const float (&row)[4]) { return all_finite(row); });
}

// The translation column of a 3x4 pose.
inline Vec3 translation_of(const Matrix34& p) noexcept { return {p.m[0][3], p.m[1][3], p.m[2][3]}; }

// Sets the translation column of a 3x4 pose.
inline void set_translation(Matrix34& p, const Vec3& t) noexcept {
    p.m[0][3] = t.x;
    p.m[1][3] = t.y;
    p.m[2][3] = t.z;
}

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

// A 4x4 matrix stored as 16 floats, row-major (row vectors: the translation
// is row 3).
inline constexpr std::size_t kMatrix4Rows    = 4;
inline constexpr std::size_t kMatrix44Floats = kMatrix4Rows * kMatrix4Rows;
inline constexpr std::size_t kTranslationRow = 3;
using Matrix44                               = std::array<float, kMatrix44Floats>;

// A 4x4 wherever it lives: a Matrix44 or a game's float[16] converts to it.
using Matrix44View    = std::span<const float, kMatrix44Floats>;
using Matrix44MutView = std::span<float, kMatrix44Floats>;

// The three axes of a rigid 4x4: the row of a model matrix (right and up
// axes scaled to the object's size, then its forward axis), and the column
// of a world-to-view matrix that holds the camera's axis in world space.
inline constexpr std::size_t kRightAxis   = 0;
inline constexpr std::size_t kUpAxis      = 1;
inline constexpr std::size_t kForwardAxis = 2;

// Element (row, col) of a 4x4 stored as above.
constexpr float element4(Matrix44View m, std::size_t row, std::size_t col) noexcept {
    return m[row * kMatrix4Rows + col];
}

// The camera position of a rigid world-to-view matrix `v` (4x4, row
// vectors): minus the translation rotated back, that is minus the dot
// product of the translation with each rotation row.
inline Vec3 camera_position(Matrix44View v) noexcept {
    const auto back = [v](std::size_t row) {
        float sum = 0.0f;
        for (std::size_t col = 0; col < 3; ++col)
            sum += element4(v, kTranslationRow, col) * element4(v, row, col);
        return -sum;
    };
    return {back(0), back(1), back(2)};
}

// Sets the 4x4 `m` to identity.
inline void identity(Matrix44MutView m) noexcept {
    for (std::size_t row = 0; row < kMatrix4Rows; ++row) {
        for (std::size_t col = 0; col < kMatrix4Rows; ++col)
            m[row * kMatrix4Rows + col] = row == col ? 1.0f : 0.0f;
    }
}

// The 4x4 identity.
inline Matrix44 identity44() noexcept {
    Matrix44 m{};
    identity(m);
    return m;
}

// Writes x, y, z of `row`; the fourth column keeps its value.
inline void set_row3(Matrix44& m, std::size_t row, const Vec3& v) noexcept {
    m[row * kMatrix4Rows + 0] = v.x;
    m[row * kMatrix4Rows + 1] = v.y;
    m[row * kMatrix4Rows + 2] = v.z;
}

// Rows 0 to 2 of `column`.
inline Vec3 column3(const Matrix44& m, std::size_t column) noexcept {
    return {element4(m, 0, column), element4(m, 1, column), element4(m, 2, column)};
}

} // namespace omvr::util
