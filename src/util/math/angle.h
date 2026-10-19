// OpenMotherVR-Provenance: clean
#pragma once

#include <numbers>

namespace omvr::util {

// Radians per degree (pi / 180 in float).
inline constexpr float kDegToRad = std::numbers::pi_v<float> / 180.0f;

// Degrees per radian (180 / pi, rounded to float). Written as a literal: it
// is one float above 180.0f / pi_v<float>, and the angles logged and
// compared with it keep their values.
inline constexpr float kRadToDeg = 57.29578f;

} // namespace omvr::util
