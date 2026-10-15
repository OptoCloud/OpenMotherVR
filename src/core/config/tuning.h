#pragma once

namespace omvr::core::config {

// Stereo anti-aliasing "Auto" runs only up to this pixel scale: above it the
// eyes are supersampled enough that it costs about 0.6 ms GPU for little gain.
inline constexpr float kStereoAaMaxAutoScale = 1.3f;

} // namespace omvr::core::config
