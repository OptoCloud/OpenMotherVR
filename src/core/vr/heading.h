#pragma once

#include "util/math/vec.h"

namespace omvr::core::vr {

// A heading about +Y in radians, and whether it was kept from before
// because the forward axis had no horizontal direction.
struct Heading {
    float yaw_rad = 0.0f;
    bool vertical = false;
};

// The heading of a forward axis given by its horizontal components (x, z).
// Looking straight up or down leaves no heading: `previous_yaw_rad` is kept
// (03 §2.1, 09 §5.5).
Heading heading_from_forward(float forward_x, float forward_z, float previous_yaw_rad) noexcept;

// The same for the orientation `q` (runtime convention: +Y up, -Z forward).
Heading heading_of(const util::Quat& q, float previous_yaw_rad) noexcept;

// A rotation by `yaw_rad` about +Y: the orientation of a level reference.
util::Quat yaw_rotation(float yaw_rad) noexcept;

// The world-Y component of the up axis of `q`.
float up_y_of(const util::Quat& q) noexcept;

// An up axis with world-Y component `up_y` is within about 45 degrees of
// vertical: a headset being worn rather than lying on a desk (03 §2.3).
bool is_upright(float up_y) noexcept;

} // namespace omvr::core::vr
