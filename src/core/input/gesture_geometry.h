// OpenMotherVR-Provenance: clean
#pragma once

#include "core/input/types.h"
#include "util/math/vec.h"

#include <cstddef>
#include <optional>

// Building blocks the gestures of 22 §8 and §9 share: where a point is
// relative to the head, whether a hand is near a point, and the input-focus
// rule every gesture follows (22 §8.1). Pure values; the gesture that uses
// them owns their state.
namespace omvr::core::input {

// The head frame of 22 §8.1: origin at a head position, rotated by the
// inverse of a head yaw only, so x is right, y up and z back whatever the
// head's pitch and roll.
class HeadFrame {
public:
    HeadFrame(const Vec3& origin, float yaw) noexcept;

    // `p` (tracking space) in this frame.
    [[nodiscard]] Vec3 local(const Vec3& p) const noexcept;
    // The tracking-space point at `offset` in this frame.
    [[nodiscard]] Vec3 world(const Vec3& offset) const noexcept;

private:
    Vec3 origin_;
    float cos_ = 1.0f, sin_ = 0.0f;
};

// The head frame of this frame's head pose; nullopt when the head pose is
// not valid.
[[nodiscard]] std::optional<HeadFrame> head_frame(const BodyPoses& poses) noexcept;

// Hand `hand` (kLeftHand, kRightHand) in the head frame of this frame;
// nullopt when either pose is not valid (22 §8.1: a hand without a valid
// pose never takes part in a gesture).
[[nodiscard]] std::optional<Vec3> hand_in_head_frame(const BodyPoses& poses, std::size_t hand) noexcept;

// Hand `hand` within `radius` of the head position (04 §5.10, 22 §8.2,
// §8.3); false without valid poses.
[[nodiscard]] bool hand_near_head(const BodyPoses& poses, std::size_t hand, float radius) noexcept;

// 22 §8.1: no gesture fires while the runtime has no input focus, nor in
// the first kGestureFocusSettle after focus returns. One per gesture user;
// call open() once per controls write.
class FocusGate {
public:
    // Whether gestures may fire now.
    bool open(bool focused, TimePoint now) noexcept;

private:
    bool focused_ = false;
    TimePoint since_{};
};

} // namespace omvr::core::input
