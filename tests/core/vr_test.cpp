// core/vr: heading and the vertical rule, the game pose conversion, FOV
// tangents and crop bounds, pose hold rules, pose age and runtime choice.

#include "core/diag/tuning.h"
#include "core/vr/fov.h"
#include "core/vr/heading.h"
#include "core/vr/pose_age.h"
#include "core/vr/pose_convert.h"
#include "core/vr/pose_hold.h"
#include "core/vr/runtime_choice.h"
#include "core/vr/tuning.h"

#include <doctest/doctest.h>

#include <cmath>
#include <numbers>
#include <optional>

namespace {

using namespace omvr::core::vr;
using omvr::core::config::VRRuntime;
using omvr::util::Quat;
using omvr::util::Vec3;
using Chosen = DetectResult::Chosen;

TEST_CASE("heading from the forward axis, and the vertical rule") {
    const Heading ahead = heading_from_forward(0.0f, 1.0f, 0.3f);
    CHECK(ahead.yaw_rad == 0.0f);
    CHECK_FALSE(ahead.vertical);

    const Heading right = heading_from_forward(1.0f, 0.0f, 0.0f);
    CHECK(right.yaw_rad == doctest::Approx(std::numbers::pi_v<float> / 2));

    // Looking straight up or down: the previous yaw is kept.
    const Heading up = heading_from_forward(kMinForwardLength / 2, 0.0f, 0.3f);
    CHECK(up.vertical);
    CHECK(up.yaw_rad == 0.3f);
}

TEST_CASE("heading of a quaternion; yaw rotation; upright") {
    CHECK(heading_of(Quat{}, 1.0f).yaw_rad == 0.0f);
    CHECK(heading_of(yaw_rotation(0.5f), 0.0f).yaw_rad == doctest::Approx(0.5f));

    const Quat q = yaw_rotation(1.2f);
    CHECK(q.x == 0.0f);
    CHECK(q.z == 0.0f);
    CHECK(q.y == doctest::Approx(std::sin(0.6f)));
    CHECK(q.w == doctest::Approx(std::cos(0.6f)));

    CHECK(up_y_of(Quat{}) == 1.0f);
    CHECK(is_upright(1.0f));
    CHECK(is_upright(kUprightCosine + 0.01f));
    CHECK_FALSE(is_upright(kUprightCosine));  // strictly above
}

TEST_CASE("to_game_pose negates x and y columns and scales the position") {
    const float r[3][4] = {{1, 2, 3, 4}, {5, 6, 7, 8}, {9, 10, 11, 12}};
    const auto o = to_game_pose(r, 2.0f);
    CHECK(o.m[0][0] == -1.0f);
    CHECK(o.m[0][1] == -2.0f);
    CHECK(o.m[0][2] == 3.0f);
    CHECK(o.m[0][3] == -8.0f);
    CHECK(o.m[2][3] == -24.0f);

    const float v[3] = {1, -2, 3};
    float out[3];
    to_game_velocity(v, 0.5f, out);
    CHECK(out[0] == -0.5f);
    CHECK(out[1] == 1.0f);
    CHECK(out[2] == -1.5f);
}

TEST_CASE("FOV tangents from a raw projection and from angles") {
    const FovTangents t = tangents_from_raw({-1.0f, 1.2f, -1.1f, 0.9f});
    CHECK(t.up == 0.9f);
    CHECK(t.down == 1.1f);
    CHECK(t.left == 1.0f);
    CHECK(t.right == 1.2f);

    const FovTangents a = tangents_from_angles({-0.7f, 0.8f, 0.6f, -0.5f});
    CHECK(a.up == doctest::Approx(std::tan(0.6f)));
    CHECK(a.down == doctest::Approx(std::tan(0.5f)));
    CHECK(a.left == doctest::Approx(std::tan(0.7f)));
    CHECK(a.right == doctest::Approx(std::tan(0.8f)));
}

TEST_CASE("render tangents are the widest of both eyes; symmetric forms") {
    const RenderTangents r = render_tangents({1.0f, 1.1f, 0.9f, 1.3f}, {1.2f, 0.8f, 1.4f, 0.7f});
    CHECK(r.vertical == 1.2f);
    CHECK(r.horizontal == 1.4f);

    const FovTangents s = symmetric_tangents(r);
    CHECK(s.up == 1.2f);
    CHECK(s.down == 1.2f);
    CHECK(s.left == 1.4f);
    CHECK(s.right == 1.4f);

    const FovAngles g = symmetric_angles({1.0f, 1.0f});
    const float q = std::numbers::pi_v<float> / 4;
    CHECK(g.left == doctest::Approx(-q));
    CHECK(g.right == doctest::Approx(q));
    CHECK(g.up == doctest::Approx(q));
    CHECK(g.down == doctest::Approx(-q));
}

TEST_CASE("crop bounds place each eye in its half") {
    const RawProjection p{-1.0f, 1.0f, -1.0f, 1.0f};
    const TextureBounds left = crop_bounds(0, p, {1.0f, 1.0f});
    CHECK(left.u_min == 0.0f);
    CHECK(left.v_min == 0.0f);
    CHECK(left.u_max == 0.5f);
    CHECK(left.v_max == 1.0f);

    const TextureBounds right = crop_bounds(1, {-0.5f, 1.0f, -1.0f, 1.0f}, {1.0f, 1.0f});
    CHECK(right.u_min == doctest::Approx(0.625f));
    CHECK(right.u_max == doctest::Approx(1.0f));
}

TEST_CASE("texture size scales and truncates") {
    const TextureSize s = scaled_size(1000, 800, 1.5f);
    CHECK(s.w == 1500);
    CHECK(s.h == 1200);
    const TextureSize t = scaled_size(1001, 3, 0.5f);
    CHECK(t.w == 500);
    CHECK(t.h == 1);
}

TEST_CASE("pose hold rules") {
    CHECK(head_tracking(false, true) == HeadTracking::Invalid);
    CHECK(head_tracking(true, false) == HeadTracking::Degraded);
    CHECK(head_tracking(true, true) == HeadTracking::Tracked);

    omvr::util::Pose held{Quat{}, {1, 1, 1}};
    const omvr::util::Pose located{Quat{0, 1, 0, 0}, {5, 5, 5}};
    hold_pose(held, located, false, true);
    CHECK(held.orientation.w == 1.0f);
    hold_pose(held, located, true, false);
    CHECK(held.orientation.y == 1.0f);
    CHECK(held.position.x == 1.0f);  // position degraded: kept
    hold_pose(held, located, true, true);
    CHECK(held.position.x == 5.0f);

    float m[3][4] = {};
    const float n[3][4] = {{1, 2, 3, 4}, {5, 6, 7, 8}, {9, 10, 11, 12}};
    hold_pose(m, n, true, false);
    CHECK(m[1][2] == 7.0f);
    CHECK(m[1][3] == 0.0f);
    hold_pose(m, n, true, true);
    CHECK(m[1][3] == 8.0f);

    Vec3 h{1, 2, 3};
    Vec3 l{4, 5, 6};
    hold_position(h, l, false);
    CHECK(l.x == 1.0f);  // invalid: the located one takes the held value
    Vec3 l2{7, 8, 9};
    hold_position(h, l2, true);
    CHECK(h.x == 7.0f);
}

TEST_CASE("pose age reports every kDebugLogPeriodFrames frames") {
    PoseAge age;
    constexpr std::int64_t kPeriod = 100;
    std::optional<PoseAgeReport> report;
    const auto frames = omvr::core::diag::kDebugLogPeriodFrames;
    for (std::uint32_t i = 0; i < frames; ++i) {
        std::int64_t rendered_for = 1000;
        if (i % 4 == 1) rendered_for = 900;  // one period early
        if (i % 4 == 2) rendered_for = 750;  // 2.5 periods: other
        if (i % 4 == 3) rendered_for = 0;    // unknown
        report = age.add(1000, rendered_for, kPeriod);
        if (i + 1 < frames) REQUIRE_FALSE(report.has_value());
    }
    REQUIRE(report.has_value());
    CHECK(report->frames == frames);
    CHECK(report->same == frames / 4);
    CHECK(report->one_early == frames / 4);
    CHECK(report->other == frames / 2);
    CHECK(report->known == frames * 3 / 4);
    CHECK(report->mean == doctest::Approx((0.0 + 1.0 + 2.5) / 3.0));
    // Starts over.
    CHECK_FALSE(age.add(1000, 1000, kPeriod).has_value());
}

TEST_CASE("runtime choice precedence") {
    DetectResult r;
    r.openvr_available = true;
    r.openxr_available = true;

    r.preferred = VRRuntime::None;
    CHECK(choose_runtime(r) == Chosen::None);

    r.preferred = VRRuntime::OpenXR;
    CHECK(choose_runtime(r) == Chosen::OpenXR);

    r.openxr_available = false;
    CHECK(choose_runtime(r) == Chosen::OpenVR);  // falls back

    r.forced = true;
    CHECK(choose_runtime(r) == Chosen::None);  // a forced runtime is missing: no fallback

    DetectResult only_xr;
    only_xr.openxr_available = true;
    only_xr.preferred        = VRRuntime::OpenVR;
    CHECK(choose_runtime(only_xr) == Chosen::OpenXR);

    DetectResult none;
    CHECK(choose_runtime(none) == Chosen::None);

    CHECK(std::string_view{to_string(Chosen::OpenVR)} == "OpenVR");
    CHECK(std::string_view{to_string(Chosen::OpenXR)} == "OpenXR");
    CHECK(std::string_view{to_string(Chosen::Oculus)} == "Oculus");
    CHECK(std::string_view{to_string(Chosen::None)} == "None");
}

} // namespace
