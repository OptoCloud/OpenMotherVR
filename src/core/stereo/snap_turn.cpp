#include "core/stereo/snap_turn.h"

#include "core/stereo/tuning.h"

#include <algorithm>
#include <cmath>

namespace omvr::core::stereo {

namespace {

// A full turn in the game's yaw unit.
constexpr float kFullTurn = 2.0f;
// The wrapped range is [-kHalfTurn, kHalfTurn].
constexpr float kHalfTurn = 1.0f;
// Degrees in the game's half-turn unit.
constexpr float kDegreesPerHalfTurn = 180.0f;

float wrap_half_turns(float yaw) noexcept {
    if (yaw > kHalfTurn) yaw -= kFullTurn;
    if (yaw < -kHalfTurn) yaw += kFullTurn;
    return yaw;
}

} // namespace

bool turn_out(float turn) noexcept { return turn < -kTurnThreshold || turn > kTurnThreshold; }

float snap_step(int segments) noexcept {
    return kFullTurn / static_cast<float>(std::max(segments, kMinSnapSegments));
}

float snapped_yaw(float yaw, float turn, int segments) noexcept {
    const float step = snap_step(segments);
    return wrap_half_turns(yaw + (turn < -kTurnThreshold ? -step : step));
}

float smooth_turn_share(float turn) noexcept {
    const float share = (std::fabs(turn) - kSmoothTurnDeadZone) / (1.0f - kSmoothTurnDeadZone);
    return std::clamp(share, 0.0f, 1.0f);
}

float smoothed_yaw(float yaw, float turn, float speed, float seconds) noexcept {
    const float degrees = speed * smooth_turn_share(turn) * seconds;
    return wrap_half_turns(yaw + std::copysign(degrees / kDegreesPerHalfTurn, turn));
}

} // namespace omvr::core::stereo
