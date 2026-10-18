// OpenMotherVR-Provenance: clean
#pragma once

#include <algorithm>
#include <cmath>
#include <span>

namespace omvr::util {

// No value of `values` is infinite or NaN. A Matrix44 (std::array) or any
// float array converts to the span.
inline bool all_finite(std::span<const float> values) noexcept {
    return std::ranges::all_of(values, [](float v) { return std::isfinite(v); });
}

} // namespace omvr::util
