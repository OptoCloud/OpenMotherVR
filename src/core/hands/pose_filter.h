// OpenMotherVR-Provenance: clean
#pragma once

#include "util/math/matrix.h"
#include "util/math/vec.h"

// The hand smoothing of spec 21 §3.2: an adaptive low-pass filter (the
// public "1 Euro" filter) on a pose's position and rotation, whose cut-off
// rises with the speed, so a still hand stops trembling and a moving one
// does not lag. Game thread.
namespace omvr::core::hands {

// The filter's minimum cut-off for a Hands.Smoothing value: 10 Hz at 0 to
// 1 Hz at 1, linearly (21 §3.2).
float smoothing_cutoff_hz(float smoothing) noexcept;

// The weight of the new sample in an exponential filter with cut-off
// `cutoff_hz` sampled `dt_s` apart: 1 / (1 + 1 / (2 pi fc dt)).
float smoothing_alpha(float cutoff_hz, float dt_s) noexcept;

class PoseFilter final {
public:
    // `pose` filtered, `dt_s` after the previous sample. With `smoothing`
    // 0 (off, the default) the pose passes unchanged and the filter starts
    // afresh on the next sample with smoothing on; so does a non-positive
    // `dt_s`. `world_scale` turns the pose's units back into metres for the
    // speed term.
    util::Matrix34 apply(const util::Matrix34& pose, float dt_s, float smoothing, float world_scale) noexcept;

    // The next sample starts the filter afresh (a hand that was lost, a gap).
    void reset() noexcept { primed_ = false; }

private:
    bool primed_ = false;
    util::Vec3 position_{};
    util::Quat rotation_{};
    float linear_speed_  = 0.0f; // filtered, metres per second
    float angular_speed_ = 0.0f; // filtered, radians per second
};

} // namespace omvr::core::hands
