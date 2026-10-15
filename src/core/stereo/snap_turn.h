#pragma once

// Snap turn (05 §3.2c): the camera's yaw steps by a fixed angle when the
// turn axis leaves the dead zone. Yaw is in the game's unit, half turns
// (+-1 = +-180 degrees), wrapped to [-1, 1]. Pure.
namespace omvr::core::stereo {

// The turn axis is past the snap threshold, either way.
bool turn_out(float turn) noexcept;

// One step for `segments` steps per full turn (at least kMinSnapSegments),
// in half turns. In floating point, so 3 segments step 120 degrees rather
// than 180 (04 §6).
float snap_step(int segments) noexcept;

// `yaw` after one step in the direction of `turn`, wrapped to [-1, 1].
float snapped_yaw(float yaw, float turn, int segments) noexcept;

} // namespace omvr::core::stereo
