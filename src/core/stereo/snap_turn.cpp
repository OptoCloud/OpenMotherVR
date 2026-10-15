#include "core/stereo/snap_turn.h"

#include "core/stereo/tuning.h"

#include <algorithm>

namespace omvr::core::stereo {

namespace {

// A full turn in the game's yaw unit.
constexpr float kFullTurn = 2.0f;
// The wrapped range is [-kHalfTurn, kHalfTurn].
constexpr float kHalfTurn = 1.0f;

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

} // namespace omvr::core::stereo
