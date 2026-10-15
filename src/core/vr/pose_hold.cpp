// OpenMotherVR-Provenance: clean
#include "core/vr/pose_hold.h"

namespace omvr::core::vr {

namespace {

// Rows of a 3x4 pose, and its rotation columns; column 3 is the position.
constexpr int kRows            = 3;
constexpr int kRotationColumns = 3;
constexpr int kPositionColumn  = 3;

} // namespace

HeadTracking head_tracking(bool valid, bool position_ok) noexcept {
    if (!valid) return HeadTracking::Invalid;
    return position_ok ? HeadTracking::Tracked : HeadTracking::Degraded;
}

void hold_pose(util::Pose& held, const util::Pose& located, bool orientation_valid,
               bool position_valid) noexcept {
    if (!orientation_valid) return;
    held.orientation = located.orientation;
    if (position_valid) held.position = located.position;
}

void hold_pose(float (&held)[3][4], const float (&located)[3][4], bool orientation_valid,
               bool position_valid) noexcept {
    if (!orientation_valid) return;
    for (int r = 0; r < kRows; ++r) {
        for (int c = 0; c < kRotationColumns; ++c) held[r][c] = located[r][c];
        if (position_valid) held[r][kPositionColumn] = located[r][kPositionColumn];
    }
}

void hold_position(util::Vec3& held, util::Vec3& located, bool valid) noexcept {
    if (valid)
        held = located;
    else
        located = held;
}

} // namespace omvr::core::vr
