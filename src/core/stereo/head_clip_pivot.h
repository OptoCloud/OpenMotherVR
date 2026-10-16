// OpenMotherVR-Provenance: clean
#pragma once

#include "util/math/matrix.h"
#include "util/math/vec.h"

namespace omvr::core::stereo {

// The pivot HeadClipFade measures the eye against (05 §7.2): the fade
// darkens from 0.5 m between eye and pivot and is black at 0.6 m, which is
// meant to stop the player's real head from leaving the body. In
// first-person scenes where an animation drives the view (the game moves
// the camera with the character's animated head, 23 §2.3), the eye moves
// away from the pivot without the player moving, and the view goes black
// for the whole scene. Only the player's own displacement since the last
// recenter should count: when the game's eye-to-pivot distance exceeds the
// real head's displacement by more than kHeadClipAnimationMargin, the
// pivot is moved along the same line to the real displacement's distance
// from the eye, so the distance term and the wall probe see the real
// head's movement only. Game thread (head pose, recenter and camera update
// all run there).
class HeadClipPivot final {
public:
    // The head pose handed to the game (device slot 11) and the reference
    // taken at a recenter (slot 21), in the game's convention: translation
    // in column 3, metres times the world scale.
    void on_head_pose(const util::Matrix34& head) noexcept;
    void on_reference(const util::Matrix34& reference) noexcept;

    // The pivot to hand HeadClipFade for the game's `eye` and `pivot`.
    util::Vec3 pivot(const util::Vec3& eye, const util::Vec3& pivot) const noexcept;

    // How far the real head is from where it was at the last recenter, in
    // the game's metres; 0 before both poses are known.
    float real_displacement() const noexcept;

    // Whether the last pivot() moved the game's pivot.
    bool adjusted() const noexcept { return adjusted_; }

private:
    util::Vec3 head_{};
    util::Vec3 reference_{};
    bool has_head_         = false;
    bool has_reference_    = false;
    mutable bool adjusted_ = false;
};

} // namespace omvr::core::stereo
