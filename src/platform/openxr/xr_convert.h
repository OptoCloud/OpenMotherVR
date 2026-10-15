#pragma once

// Conversions between OpenXR's structs and the project's. Only the .cpp
// files of this adapter include it.

#include "core/vr/types.h"
#include "platform/openxr/xr_headers.h"
#include "util/math/pose.h"

namespace omvr::platform::openxr {

using util::Pose;
using util::Quat;
using util::Vec2;
using util::Vec3;

// Both orientation and position valid: what a re-anchor or a grip pose
// needs (09 §5.5).
inline constexpr XrSpaceLocationFlags kOrientationAndPosition =
    XR_SPACE_LOCATION_ORIENTATION_VALID_BIT | XR_SPACE_LOCATION_POSITION_VALID_BIT;

// The identity pose: a space at its reference's origin.
inline constexpr XrPosef kIdentityPose{{0, 0, 0, 1}, {0, 0, 0}};

inline Pose from_xr(const XrPosef& p) noexcept {
    return {{p.orientation.x, p.orientation.y, p.orientation.z, p.orientation.w},
            {p.position.x, p.position.y, p.position.z}};
}

inline XrPosef to_xr(const Pose& p) noexcept {
    return {{p.orientation.x, p.orientation.y, p.orientation.z, p.orientation.w},
            {p.position.x, p.position.y, p.position.z}};
}

inline core::vr::FovAngles from_xr(const XrFovf& f) noexcept {
    return {f.angleLeft, f.angleRight, f.angleUp, f.angleDown};
}

inline XrFovf to_xr(const core::vr::FovAngles& f) noexcept { return {f.left, f.right, f.up, f.down}; }

} // namespace omvr::platform::openxr
