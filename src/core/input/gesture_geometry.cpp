// OpenMotherVR-Provenance: clean
#include "core/input/gesture_geometry.h"

#include "core/input/tuning.h"

#include <cmath>

namespace omvr::core::input {

// A head facing yaw y looks along (-sin y, 0, -cos y) (rotation about +Y);
// the inverse rotation takes that direction to (0, 0, -1), in front.
HeadFrame::HeadFrame(const Vec3& origin, float yaw) noexcept
    : origin_(origin), cos_(std::cos(yaw)), sin_(std::sin(yaw)) {}

Vec3 HeadFrame::local(const Vec3& p) const noexcept {
    const Vec3 d = util::sub(p, origin_);
    return {d.x * cos_ - d.z * sin_, d.y, d.x * sin_ + d.z * cos_};
}

Vec3 HeadFrame::world(const Vec3& o) const noexcept {
    return {origin_.x + o.x * cos_ + o.z * sin_, origin_.y + o.y, origin_.z - o.x * sin_ + o.z * cos_};
}

std::optional<HeadFrame> head_frame(const BodyPoses& poses) noexcept {
    if (!poses.head_valid) return std::nullopt;
    return HeadFrame{poses.head, poses.head_yaw};
}

std::optional<Vec3> hand_in_head_frame(const BodyPoses& poses, std::size_t hand) noexcept {
    if (hand >= poses.hands.size() || !poses.hand_valid[hand]) return std::nullopt;
    const auto frame = head_frame(poses);
    if (!frame) return std::nullopt;
    return frame->local(poses.hands[hand]);
}

bool hand_near_head(const BodyPoses& poses, std::size_t hand, float radius) noexcept {
    if (hand >= poses.hands.size() || !poses.hand_valid[hand] || !poses.head_valid) return false;
    return util::length(util::sub(poses.hands[hand], poses.head)) < radius;
}

bool FocusGate::open(bool focused, TimePoint now) noexcept {
    if (focused && !focused_) since_ = now;
    focused_ = focused;
    return focused && now - since_ >= kGestureFocusSettle;
}

} // namespace omvr::core::input
