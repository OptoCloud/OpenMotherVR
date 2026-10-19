// OpenMotherVR-Provenance: clean
#include "core/stereo/comfort_view.h"

#include "core/stereo/tuning.h"
#include "util/math/angle.h"
#include "util/time/steady.h"

#include <algorithm>
#include <cmath>
#include <numbers>

namespace omvr::core::stereo {

namespace {

using util::kDegToRad;
using util::Seconds;

// Radii no pixel reaches: a pixel's angle from the forward axis is below
// 90 degrees.
constexpr float kUnreachableRad = std::numbers::pi_v<float>;

// The share of `full` that `elapsed` is, clamped to [0, 1].
float share_of(std::chrono::steady_clock::duration elapsed, std::chrono::milliseconds full) noexcept {
    return std::clamp(Seconds(elapsed).count() / Seconds(full).count(), 0.0f, 1.0f);
}

} // namespace

VignetteRadii vignette_radii(float strength) noexcept {
    if (!(strength > 0.0f)) return {kUnreachableRad, kUnreachableRad + kVignetteBandDeg * kDegToRad};
    const float k     = std::min(strength, 1.0f);
    const float inner = kVignetteInnerDeg - kVignetteInnerPerStrengthDeg * k;
    return {inner * kDegToRad, (inner + kVignetteBandDeg) * kDegToRad};
}

// Spec 22 §5: the pixel's direction tangents ((2u - 1) * tan_h,
// (1 - 2v) * tan_v); its angle is atan of their length.
float vignette_angle(float u, float v, float tan_h, float tan_v) noexcept {
    const float tx = (2.0f * u - 1.0f) * tan_h;
    const float ty = (1.0f - 2.0f * v) * tan_v;
    return std::atan(std::hypot(tx, ty));
}

// Smoothstep from the inner to the outer radius, inverted.
float vignette_factor(float angle_rad, const VignetteRadii& radii) noexcept {
    const float t =
        std::clamp((angle_rad - radii.inner_rad) / (radii.outer_rad - radii.inner_rad), 0.0f, 1.0f);
    return 1.0f - t * t * (3.0f - 2.0f * t);
}

float VignetteRamp::advance(float target, std::chrono::steady_clock::duration dt) noexcept {
    if (target > 0.0f) {
        full_  = target;
        share_ = std::min(1.0f, share_ + share_of(dt, kVignetteRiseTime));
    } else {
        share_ = std::max(0.0f, share_ - share_of(dt, kVignetteFallTime));
    }
    return share_ * full_;
}

float SnapFade::darkness(std::uint32_t steps, ComfortTime now) noexcept {
    if (steps != seen_) {
        seen_  = steps;
        start_ = now; // a further step restarts from black
    }
    if (!start_) return 0.0f;
    const float passed = share_of(now - *start_, kSnapFadeTime);
    if (passed >= 1.0f) {
        start_.reset();
        return 0.0f;
    }
    return 1.0f - passed;
}

std::optional<VignetteConstants> ComfortView::frame(ComfortTime now, bool head_tracked, float tan_h,
                                                    float tan_v) noexcept {
    const auto dt = last_frame_ ? now - *last_frame_ : std::chrono::steady_clock::duration::zero();
    last_frame_   = now;
    // The fade runs on in screen frames so a step there is not shown later.
    const float darkness = fade_.darkness(steps_.load(std::memory_order_relaxed), now);
    if (!head_tracked) {
        // Spec 22 §5, §4.3: screen frames are never darkened.
        ramp_.reset();
        return std::nullopt;
    }
    const float strength = ramp_.advance(target_.load(std::memory_order_relaxed), dt);
    if (strength <= 0.0f && darkness <= 0.0f) return std::nullopt;
    const VignetteRadii radii = vignette_radii(strength);
    VignetteConstants c;
    c.tan_h      = tan_h;
    c.tan_v      = tan_v;
    c.inner_rad  = radii.inner_rad;
    c.outer_rad  = radii.outer_rad;
    c.brightness = 1.0f - darkness;
    return c;
}

} // namespace omvr::core::stereo
