#pragma once

// Snap turn (05 §3.2c): the camera's yaw steps by a fixed angle when the
// turn axis leaves the dead zone. Smooth turn (spec 22 §4.2): the yaw
// changes at a rate in degrees per second. Yaw is in the game's unit, half
// turns (+-1 = +-180 degrees), wrapped to [-1, 1]. Pure.
namespace omvr::core::stereo {

// The turn axis is past the snap threshold, either way.
bool turn_out(float turn) noexcept;

// One step for `segments` steps per full turn (at least kMinSnapSegments),
// in half turns. In floating point, so 3 segments step 120 degrees rather
// than 180 (04 §6).
float snap_step(int segments) noexcept;

// `yaw` after one step in the direction of `turn`, wrapped to [-1, 1].
float snapped_yaw(float yaw, float turn, int segments) noexcept;

// The share of full smooth-turn speed for `turn`: 0 inside
// kSmoothTurnDeadZone, rising linearly to 1 at full deflection.
float smooth_turn_share(float turn) noexcept;

// `yaw` after `seconds` of smooth turn at `speed` degrees per second, scaled
// by the turn's share and to the right for a positive turn, wrapped to
// [-1, 1].
float smoothed_yaw(float yaw, float turn, float speed, float seconds) noexcept;

} // namespace omvr::core::stereo
