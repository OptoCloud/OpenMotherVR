#pragma once

#include "platform/openxr/xr_headers.h"
#include "util/math/pose.h"

namespace omvr::platform::openxr {

using util::Pose;
using util::Quat;
using util::Vec2;
using util::Vec3;

inline Pose from_xr(const XrPosef& p) noexcept {
    return {{p.orientation.x, p.orientation.y, p.orientation.z, p.orientation.w},
            {p.position.x, p.position.y, p.position.z}};
}

inline XrPosef to_xr(const Pose& p) noexcept {
    return {{p.orientation.x, p.orientation.y, p.orientation.z, p.orientation.w},
            {p.position.x, p.position.y, p.position.z}};
}

} // namespace omvr::platform::openxr
