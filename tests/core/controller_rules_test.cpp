// OpenMotherVR-Provenance: clean
// core/input controller rules both VR backends apply: the scheme a
// controller picks, the mapping a scheme reads with, and the near-the-head
// test. Expected values are what the backends' own copies gave before the
// rules moved here (04 §5.5, §5.10).

#include "core/input/controller_rules.h"

#include <doctest/doctest.h>

namespace {

using namespace omvr::core::input;

constexpr auto kSvi    = ControllerSource::SteamVrInput;
constexpr auto kLegacy = ControllerSource::OpenVrLegacy;
constexpr auto kXr     = ControllerSource::OpenXr;

// The right hand `distance` metres in front of the head at the origin.
BodyPoses right_hand_at(float distance, bool head_valid, bool hand_valid) {
    BodyPoses p;
    p.head                   = {0.0f, 0.0f, 0.0f};
    p.head_valid             = head_valid;
    p.hands[kRightHand]      = {0.0f, 0.0f, -distance};
    p.hand_valid[kRightHand] = hand_valid;
    return p;
}

TEST_CASE("controller rules: SteamVR Input controller types pick the scheme") {
    CHECK(scheme_for(kSvi, "vive_controller") == ControlScheme::Touchpad);
    CHECK(scheme_for(kSvi, "knuckles") == ControlScheme::Index);
    CHECK(scheme_for(kSvi, "frame_controller") == ControlScheme::Frame);
    CHECK(scheme_for(kSvi, "oculus_touch") == ControlScheme::Thumbstick);
    CHECK(scheme_for(kSvi, "holographic_controller") == ControlScheme::Thumbstick);
    CHECK(scheme_for(kSvi, "some_other_controller") == ControlScheme::Thumbstick);
    CHECK(scheme_for(kSvi, "") == ControlScheme::Thumbstick);
}

TEST_CASE("controller rules: OpenXR profiles pick the scheme, Index the thumbstick one") {
    CHECK(scheme_for(kXr, "/interaction_profiles/htc/vive_controller") == ControlScheme::Touchpad);
    CHECK(scheme_for(kXr, "/interaction_profiles/valve/frame_controller_valve") == ControlScheme::Frame);
    CHECK(scheme_for(kXr, "/interaction_profiles/valve/index_controller") == ControlScheme::Thumbstick);
    CHECK(scheme_for(kXr, "/interaction_profiles/oculus/touch_controller") == ControlScheme::Thumbstick);
    CHECK(scheme_for(kXr, "/interaction_profiles/microsoft/motion_controller") == ControlScheme::Thumbstick);
    CHECK(scheme_for(kXr, "/interaction_profiles/khr/simple_controller") == ControlScheme::Thumbstick);
    CHECK(scheme_for(kXr, "") == ControlScheme::Thumbstick);
}

TEST_CASE("controller rules: OpenVR's legacy path is always the touchpad scheme") {
    CHECK(scheme_for(kLegacy, "") == ControlScheme::Touchpad);
    CHECK(scheme_for(kLegacy, "oculus_touch") == ControlScheme::Touchpad);
}

TEST_CASE("controller rules: only the touchpad scheme reads with the touchpad mapping") {
    const HandInput left{}, right{};
    CHECK(scheme_controls(ControlScheme::Touchpad, left, right).touchpad_scheme);
    CHECK_FALSE(scheme_controls(ControlScheme::Thumbstick, left, right).touchpad_scheme);
    CHECK_FALSE(scheme_controls(ControlScheme::Index, left, right).touchpad_scheme);
    CHECK_FALSE(scheme_controls(ControlScheme::Frame, left, right).touchpad_scheme);
}

TEST_CASE("controller rules: near the head is within 0.2 m, with a valid right hand") {
    for (const auto head : {HeadPosition::ThisFrame, HeadPosition::Held}) {
        CHECK(right_hand_near_head(right_hand_at(0.1f, true, true), head));
        CHECK_FALSE(right_hand_near_head(right_hand_at(0.3f, true, true), head));
        CHECK_FALSE(right_hand_near_head(right_hand_at(0.1f, true, false), head));
    }
    // Only the right hand counts.
    BodyPoses left_near             = right_hand_at(0.3f, true, true);
    left_near.hands[kLeftHand]      = {0.0f, 0.0f, -0.1f};
    left_near.hand_valid[kLeftHand] = true;
    CHECK_FALSE(right_hand_near_head(left_near, HeadPosition::ThisFrame));
}

TEST_CASE("controller rules: a head without this frame's pose counts only when it is held") {
    // OpenVR: a headset that lost tracking can report a stale or zeroed
    // matrix, so its head does not count.
    CHECK_FALSE(right_hand_near_head(right_hand_at(0.1f, false, true), HeadPosition::ThisFrame));
    // OpenXR: the head is the last valid one, held.
    CHECK(right_hand_near_head(right_hand_at(0.1f, false, true), HeadPosition::Held));
    CHECK_FALSE(right_hand_near_head(right_hand_at(0.3f, false, true), HeadPosition::Held));
}

} // namespace
