// OpenMotherVR-Provenance: clean
#include "core/hands/pose_filter.h"

#include "core/hands/tuning.h"
#include "util/math/pose.h"
#include "util/math/quat.h"

#include <numbers>

namespace omvr::core::hands {

namespace {

util::Matrix34 to_matrix(const util::Quat& q, const util::Vec3& p) noexcept {
    util::Matrix34 m{};
    util::pose_matrix({q, p}, m.m);
    return m;
}

} // namespace

float smoothing_cutoff_hz(float smoothing) noexcept {
    return kSmoothingCutoffAtZeroHz + (kSmoothingCutoffAtOneHz - kSmoothingCutoffAtZeroHz) * smoothing;
}

float smoothing_alpha(float cutoff_hz, float dt_s) noexcept {
    const float tau = 1.0f / (2 * std::numbers::pi_v<float> * cutoff_hz);
    return 1.0f / (1.0f + tau / dt_s);
}

util::Matrix34 PoseFilter::apply(const util::Matrix34& pose, float dt_s, float smoothing,
                                 float world_scale) noexcept {
    const util::Vec3 p = util::translation_of(pose);
    const util::Quat q = util::quat_from_rotation(pose);
    if (!(smoothing > 0.0f) || !(dt_s > 0.0f) || !primed_) {
        primed_        = smoothing > 0.0f;
        position_      = p;
        rotation_      = q;
        linear_speed_  = 0.0f;
        angular_speed_ = 0.0f;
        return pose;
    }
    const float metres = world_scale > 0.0f ? world_scale : 1.0f;
    // The speeds of this sample, themselves low-passed (the filter's
    // derivative stage), then the cut-offs they raise.
    const float speed_alpha = smoothing_alpha(kSpeedCutoffHz, dt_s);
    const float linear      = util::length(util::sub(p, position_)) / metres / dt_s;
    const float angular     = util::angle_between(rotation_, q) / dt_s;
    linear_speed_ += (linear - linear_speed_) * speed_alpha;
    angular_speed_ += (angular - angular_speed_) * speed_alpha;
    const float min_cutoff = smoothing_cutoff_hz(smoothing);
    const float a_pos = smoothing_alpha(min_cutoff + kSmoothingBetaPerMetrePerSecond * linear_speed_, dt_s);
    const float a_rot = smoothing_alpha(min_cutoff + kSmoothingBetaPerRadianPerSecond * angular_speed_, dt_s);
    position_         = util::lerp(position_, p, a_pos);
    rotation_         = util::nlerp(rotation_, q, a_rot);
    return to_matrix(rotation_, position_);
}

} // namespace omvr::core::hands
