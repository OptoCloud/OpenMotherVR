#pragma once

#include "util/math/vec.h"

#include <cstddef>

namespace omvr::util {

// Row-major 3x4, m[r][3] = translation.
struct Matrix34 {
    float m[3][4];
};

// A 4x4 matrix stored as 16 floats, row-major (row vectors: the translation
// is row 3).
inline constexpr std::size_t kMatrix4Rows     = 4;
inline constexpr std::size_t kMatrix4Elements = kMatrix4Rows * kMatrix4Rows;
inline constexpr std::size_t kTranslationRow  = 3;

// Element (row, col) of a 4x4 stored as above.
constexpr float element4(const float* m, std::size_t row, std::size_t col) noexcept {
    return m[row * kMatrix4Rows + col];
}

// The camera position of a rigid world-to-view matrix `v` (4x4, row
// vectors): minus the translation rotated back, that is minus the dot
// product of the translation with each rotation row.
inline Vec3 camera_position(const float* v) noexcept {
    const auto back = [v](std::size_t row) {
        float sum = 0.0f;
        for (std::size_t col = 0; col < 3; ++col)
            sum += element4(v, kTranslationRow, col) * element4(v, row, col);
        return -sum;
    };
    return {back(0), back(1), back(2)};
}

// Sets the 4x4 `m` to identity.
inline void identity(float* m) noexcept {
    for (std::size_t row = 0; row < kMatrix4Rows; ++row) {
        for (std::size_t col = 0; col < kMatrix4Rows; ++col)
            m[row * kMatrix4Rows + col] = row == col ? 1.0f : 0.0f;
    }
}

} // namespace omvr::util
