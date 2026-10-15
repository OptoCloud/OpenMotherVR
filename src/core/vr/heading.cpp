// OpenMotherVR-Provenance: clean
#include "core/vr/heading.h"

#include "core/vr/tuning.h"

#include <cmath>

namespace omvr::core::vr {

Heading heading_from_forward(float forward_x, float forward_z, float previous_yaw_rad) noexcept {
    const bool vertical = std::sqrt(forward_x * forward_x + forward_z * forward_z) < kMinForwardLength;
    return {vertical ? previous_yaw_rad : std::atan2(forward_x, forward_z), vertical};
}

Heading heading_of(const util::Quat& q, float previous_yaw_rad) noexcept {
    // The horizontal components of the rotated forward axis.
    const float forward_x = 2.0f * (q.w * q.y + q.x * q.z);
    const float forward_z = 1.0f - 2.0f * (q.x * q.x + q.y * q.y);
    return heading_from_forward(forward_x, forward_z, previous_yaw_rad);
}

util::Quat yaw_rotation(float yaw_rad) noexcept {
    const float half = yaw_rad * 0.5f;
    return {0, std::sin(half), 0, std::cos(half)};
}

float up_y_of(const util::Quat& q) noexcept { return 1 - 2 * (q.x * q.x + q.z * q.z); }

bool is_upright(float up_y) noexcept { return up_y > kUprightCosine; }

} // namespace omvr::core::vr
