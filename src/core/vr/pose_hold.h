// OpenMotherVR-Provenance: clean
#pragma once

#include "util/math/pose.h"

// What the game gets while tracking is lost or degraded: the last valid
// pose, part by part (03 §1, 09 §5.5).
namespace omvr::core::vr {

// How well the headset is tracked in the latest pose.
enum class HeadTracking { Invalid, Degraded, Tracked };

// `valid`: the pose can be used at all; `position_ok`: its position too.
HeadTracking head_tracking(bool valid, bool position_ok) noexcept;

// Updates the held pose from a located one: the orientation when it is
// valid, the position only when the orientation and the position both are.
void hold_pose(util::Pose& held, const util::Pose& located, bool orientation_valid,
               bool position_valid) noexcept;

// The same on row-major 3x4 matrices (rotation in columns 0..2, position in
// column 3).
void hold_pose(float (&held)[3][4], const float (&located)[3][4], bool orientation_valid,
               bool position_valid) noexcept;

// An eye position that is this frame's becomes the held one; otherwise the
// held one replaces it.
void hold_position(util::Vec3& held, util::Vec3& located, bool valid) noexcept;

} // namespace omvr::core::vr
