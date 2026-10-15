#pragma once

namespace omvr::core::vr {

// Debug: where `hand` is relative to `head`, in the head's own frame (+X
// right, +Y up, -Z forward), metres. Both are runtime-convention 3x4 poses
// (unconverted, unscaled).
inline void head_local_offset(const float (&head)[3][4], const float (&hand)[3][4], float (&out)[3]) noexcept {
    const float d[3] = {hand[0][3] - head[0][3], hand[1][3] - head[1][3], hand[2][3] - head[2][3]};
    for (int c = 0; c < 3; ++c) out[c] = head[0][c] * d[0] + head[1][c] * d[1] + head[2][c] * d[2];
}

} // namespace omvr::core::vr
