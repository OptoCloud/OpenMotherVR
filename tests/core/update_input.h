// OpenMotherVR-Provenance: clean
#pragma once

#include "core/stereo/stereo_update.h"

#include <chrono>

namespace omvr::tests {

// A stereo update with a pad and a first-person camera: the turn axis at
// `turn`, the camera's yaw at `yaw` and the forward movement at `move_y`.
inline core::stereo::UpdateInput pad_input(std::chrono::steady_clock::time_point now, float turn,
                                           float yaw = 0.0f, float move_y = 0.0f) {
    core::stereo::UpdateInput in;
    in.now           = now;
    in.pad           = true;
    in.turn          = turn;
    in.move_y        = move_y;
    in.follow_active = true;
    in.follow_yaw    = yaw;
    return in;
}

} // namespace omvr::tests
