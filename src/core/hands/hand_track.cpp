// OpenMotherVR-Provenance: clean
#include "core/hands/hand_track.h"

#include <algorithm>

namespace omvr::core::hands {

float Blend::step(Seconds dt, bool target_on) noexcept {
    if (target_on) {
        value_ = std::min(1.0f, value_ + dt / kBlendIn);
    } else {
        value_ = std::max(0.0f, value_ - dt / kBlendOut);
    }
    return value_;
}

HandTrack::Change HandTrack::update(Seconds dt, const std::optional<HeadLocalPose>& grip,
                                    const std::optional<HeadLocalPose>& aim) noexcept {
    const bool was_tracked = tracked_;
    tracked_               = grip.has_value();
    if (tracked_) {
        grip_     = *grip;
        has_grip_ = true;
        if (aim) {
            aim_     = *aim;
            has_aim_ = true;
        }
        lost_for_ = Seconds{0.0f};
        weight_.step(dt, true);
    } else {
        // Held for kLostHold at the weight it had, then blended out.
        lost_for_ += dt;
        if (lost_for_ > kLostHold) weight_.step(std::min(dt, lost_for_ - kLostHold), false);
    }
    if (tracked_ == was_tracked) return Change::None;
    return tracked_ ? Change::Regained : Change::Lost;
}

} // namespace omvr::core::hands
