// OpenMotherVR-Provenance: clean
#include "core/stereo/head_clip_pivot.h"

#include "core/stereo/tuning.h"

#include <cmath>

namespace omvr::core::stereo {

namespace {

using util::translation_of;

// The distance between `a` and `b`, through std::hypot.
float hypot_distance(const util::Vec3& a, const util::Vec3& b) noexcept {
    return std::hypot(a.x - b.x, a.y - b.y, a.z - b.z);
}

} // namespace

void HeadClipPivot::on_head_pose(const util::Matrix34& head) noexcept {
    head_     = translation_of(head);
    has_head_ = true;
}

void HeadClipPivot::on_reference(const util::Matrix34& reference) noexcept {
    reference_     = translation_of(reference);
    has_reference_ = true;
}

float HeadClipPivot::real_displacement() const noexcept {
    return has_head_ && has_reference_ ? hypot_distance(head_, reference_) : 0.0f;
}

bool HeadClipPivot::real_head_still() const noexcept {
    return has_head_ && has_reference_ && real_displacement() < kHeadClipStillRadius;
}

util::Vec3 HeadClipPivot::pivot(const util::Vec3& eye, const util::Vec3& pivot) const noexcept {
    adjusted_ = false;
    if (!has_head_ || !has_reference_) return pivot;
    const float game = hypot_distance(eye, pivot);
    const float real = real_displacement();
    if (!(game > real + kHeadClipAnimationMargin)) return pivot;
    adjusted_     = true;
    const float s = real / game;
    return {eye.x + (pivot.x - eye.x) * s, eye.y + (pivot.y - eye.y) * s, eye.z + (pivot.z - eye.z) * s};
}

} // namespace omvr::core::stereo
