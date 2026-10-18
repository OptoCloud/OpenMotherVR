// OpenMotherVR-Provenance: clean
#pragma once

#include "core/hands/tuning.h"
#include "core/hands/types.h"

#include <optional>

// What a hand does when its controller stops being tracked, and when it is
// back (spec 21 §3.3). Game thread.
namespace omvr::core::hands {

// A weight that moves towards 1 over kBlendIn and towards 0 over kBlendOut:
// 1 is the tracked pose, 0 the game's own (21 §3.3, §4.4).
class Blend final {
public:
    // Moves `dt` towards `target_on`; returns the new weight.
    float step(Seconds dt, bool target_on) noexcept;
    float value() const noexcept { return value_; }
    void set(float value) noexcept { value_ = value; }

private:
    float value_ = 0.0f;
};

// One hand. While tracked, its pose relative to the head is the runtime's;
// once lost, the last tracked pose relative to the head is held for
// kLostHold, then the weight blends to 0 over kBlendOut; when tracking is
// back it blends in again over kBlendIn.
class HandTrack final {
public:
    enum class Change : std::uint8_t { None, Lost, Regained };

    // One update, `dt` after the previous one. `grip` is the hand's grip
    // pose relative to the head when the hand is tracked, nullopt when not;
    // `aim` its aim pose likewise (a runtime may report one without the
    // other). Returns whether tracking was lost or regained by this update.
    Change update(Seconds dt, const std::optional<HeadLocalPose>& grip,
                  const std::optional<HeadLocalPose>& aim) noexcept;

    // The tracked (or held) poses' weight against the game's own: 0..1.
    float weight() const noexcept { return weight_.value(); }
    // The grip and aim poses relative to the head: the latest tracked ones,
    // held while lost. Meaningful while has_grip() / has_aim().
    const HeadLocalPose& grip() const noexcept { return grip_; }
    const HeadLocalPose& aim() const noexcept { return aim_; }
    bool has_grip() const noexcept { return has_grip_; }
    bool has_aim() const noexcept { return has_aim_; }

private:
    Blend weight_;
    Seconds lost_for_{0.0f};
    HeadLocalPose grip_{};
    HeadLocalPose aim_{};
    bool tracked_  = false;
    bool has_grip_ = false;
    bool has_aim_  = false;
};

} // namespace omvr::core::hands
