// OpenMotherVR-Provenance: clean
// core/stereo seated and standing play, the eye offsets, the world scale and
// the height calibration (spec 22 §6, 21 §11.2, §11.4). Expected values are
// the spec's: v1's pose at the defaults (03 §1, §2.1), the standing vertical
// offset (head height above the floor - h) * s (22 §6.1), offsets in runtime
// metres before s (22 §6.3, 21 §11.1), the 1.0 to 2.2 m range and the 3 s
// delay (22 §6.2).

#include "core/config/launch_options.h"
#include "core/config/tuning.h"
#include "core/stereo/height_calibration.h"
#include "core/stereo/play_space.h"
#include "core/vr/pose_convert.h"
#include "fakes/fakes.h"

#include <doctest/doctest.h>

#include <bit>
#include <chrono>
#include <cstdint>
#include <cmath>
#include <numbers>
#include <string>
#include <vector>

namespace {

using namespace omvr::core::stereo;
using omvr::core::config::PlayMode;
using omvr::core::vr::to_game_pose;
using omvr::tests::RecordingLog;
using omvr::util::Matrix34;
using namespace std::chrono_literals;
using Time = std::chrono::steady_clock::time_point;

constexpr float kTol = 1e-5f;

// An OpenVR-space pose (03 §1): yaw about +Y, then a translation.
struct Raw {
    float m[3][4];
};
Raw raw_pose(float x, float y, float z, float yaw_rad = 0.0f) {
    const float s = std::sin(yaw_rad), c = std::cos(yaw_rad);
    return {{{c, 0.0f, s, x}, {0.0f, 1.0f, 0.0f, y}, {-s, 0.0f, c, z}}};
}

// What the runtime hands the play space: the game's convention at scale 1.
Matrix34 unscaled(const Raw& r) { return to_game_pose(r.m, 1.0f); }

bool same_bits(const Matrix34& a, const Matrix34& b) {
    for (int r = 0; r < 3; ++r)
        for (int c = 0; c < 4; ++c)
            if (std::bit_cast<std::uint32_t>(a.m[r][c]) != std::bit_cast<std::uint32_t>(b.m[r][c]))
                return false;
    return true;
}

// The relative translation the game computes (02 §6.3) for a yaw-0
// reference: head minus reference (the reference rotation is the neutral
// pose's, which only flips X and Y).
struct Rel {
    float x, y, z;
};
Rel relative(const Matrix34& head, const Matrix34& ref) {
    return {head.m[0][3] - ref.m[0][3], head.m[1][3] - ref.m[1][3], head.m[2][3] - ref.m[2][3]};
}

PlaySpaceSettings settings(PlayMode mode = PlayMode::Seated, float h = 0.0f, float scale = 1.0f) {
    PlaySpaceSettings s;
    s.mode                  = mode;
    s.standing_eye_height_m = h;
    s.world_scale_setting   = scale;
    return s;
}

} // namespace

TEST_CASE("play space: at the v1 values the poses are v1's, bit for bit") {
    RecordingLog log;
    PlaySpace ps{log};
    CHECK_FALSE(ps.begin_frame(settings()));
    const Raw ref  = raw_pose(0.1f, 1.2f, -0.3f, 0.7f);
    const Raw head = raw_pose(0.35f, 1.05f, 0.2f, -0.4f);
    CHECK(same_bits(ps.on_reference(unscaled(ref), 0.0f, "OpenVR"), to_game_pose(ref.m, 1.0f)));
    const HeadPoses p = ps.on_head_pose(unscaled(head), 0.0f);
    CHECK(same_bits(p.game, to_game_pose(head.m, 1.0f)));
    CHECK(same_bits(p.tracked, p.game));
    CHECK(same_bits(p.player, p.game));
    CHECK(log.lines.empty()); // v1 says nothing new

    SUBCASE("-vrscale 1.2: v1's pose at 1.2") {
        PlaySpace sw{log};
        sw.set_world_scale_switch(1.2f);
        sw.begin_frame(settings(PlayMode::Seated, 0.0f, 0.8f)); // the setting is ignored
        CHECK(sw.world_scale() == 1.2f);
        CHECK(same_bits(sw.on_reference(unscaled(ref), std::nullopt, "OpenVR"), to_game_pose(ref.m, 1.2f)));
        CHECK(same_bits(sw.on_head_pose(unscaled(head), std::nullopt).game, to_game_pose(head.m, 1.2f)));
        CHECK(log.contains("world scale 1.20 (set by -vrscale"));
    }
}

TEST_CASE("world scale: the switch wins, the setting otherwise") {
    CHECK(effective_world_scale(0.8f, std::nullopt) == 0.8f);
    CHECK(effective_world_scale(0.8f, 1.2f) == 1.2f);

    using Args = std::vector<std::wstring>;
    using omvr::core::config::world_scale_switch_in;
    CHECK_FALSE(world_scale_switch_in(Args{L"AI.exe"}));
    CHECK(world_scale_switch_in(Args{L"AI.exe", L"-vrscale", L"1.2"}) == 1.2f);
    CHECK_FALSE(world_scale_switch_in(Args{L"AI.exe", L"-vrscale", L"1.6"}));
    CHECK_FALSE(world_scale_switch_in(Args{L"AI.exe", L"-vrscale", L"abc"}));
    CHECK_FALSE(world_scale_switch_in(Args{L"AI.exe", L"-vrscale"}));
    CHECK_FALSE(world_scale_switch_in(Args{L"AI.exe", L"-vrscale", L"0.8", L"-vrscale", L"9"}));
    CHECK(world_scale_switch_in(Args{L"AI.exe", L"-vrscale", L"9", L"-VRSCALE", L"0.8"}) == 0.8f);
}

TEST_CASE("world scale: a change applies from the next frame without a jump") {
    RecordingLog log;
    PlaySpace ps{log};
    ps.begin_frame(settings());
    const Raw ref           = raw_pose(0.0f, 1.2f, 0.0f);
    const Matrix34 game_ref = ps.on_reference(unscaled(ref), 0.0f, "OpenVR"); // held by the game at s 1
    const Raw head          = raw_pose(0.1f, 1.3f, -0.2f);

    // 22 §18 item 6: WorldScale 0.8 set in play.
    ps.begin_frame(settings(PlayMode::Seated, 0.0f, 0.8f));
    CHECK(ps.world_scale() == 0.8f);
    CHECK(log.contains("world scale 0.80"));
    const Rel r = relative(ps.on_head_pose(unscaled(head), 0.0f).game, game_ref);
    // s * (head - reference), in the convention (translation negated).
    CHECK(r.x == doctest::Approx(-0.8f * 0.1f).epsilon(kTol));
    CHECK(r.y == doctest::Approx(-0.8f * 0.1f).epsilon(kTol));
    CHECK(r.z == doctest::Approx(-0.8f * -0.2f).epsilon(kTol));
    // At the reference itself the view stays at the reference: no jump.
    const Rel at_ref = relative(ps.on_head_pose(unscaled(ref), 0.0f).game, game_ref);
    CHECK(std::abs(at_ref.x) < kTol);
    CHECK(std::abs(at_ref.y) < kTol);
    CHECK(std::abs(at_ref.z) < kTol);
    // The hands' head is the runtime's pose at the new scale.
    CHECK(same_bits(ps.on_head_pose(unscaled(head), 0.0f).tracked, to_game_pose(head.m, 0.8f)));
}

TEST_CASE("seated: the height of the latest recenter, as v1") {
    RecordingLog log;
    PlaySpace ps{log};
    ps.begin_frame(settings());
    const Matrix34 ref = ps.on_reference(unscaled(raw_pose(0.0f, 0.9f, 0.0f)), 0.0f, "OpenVR"); // kneeling
    const Rel r        = relative(ps.on_head_pose(unscaled(raw_pose(0.0f, 0.9f, 0.0f)), 0.0f).game, ref);
    CHECK(std::abs(r.y) < kTol); // kneeling is the character's eye height

    SUBCASE("Body.SeatedEyeRaise raises the eye by its amount") {
        auto s               = settings();
        s.seated_eye_raise_m = 0.1f;
        ps.begin_frame(s);
        const HeadPoses p = ps.on_head_pose(unscaled(raw_pose(0.0f, 0.9f, 0.0f)), 0.0f);
        CHECK(relative(p.game, ref).y == doctest::Approx(-0.1f).epsilon(kTol)); // up is -y in the convention
        CHECK(std::abs(relative(p.player, ref).y) < kTol);                      // not the player's movement
    }
}

TEST_CASE("standing, calibrated: (height above the floor - h) * s, whenever the recenter") {
    RecordingLog log;
    PlaySpace ps{log};
    ps.begin_frame(settings(PlayMode::Standing, 1.7f));
    const float floor = 0.0f;

    // 22 §18 item 5: recentered while kneeling (eye at 1.0 m).
    const Matrix34 ref = ps.on_reference(unscaled(raw_pose(0.0f, 1.0f, 0.0f)), floor, "OpenVR");
    CHECK(log.contains("play mode Standing (standing eye height 1.70 m)"));
    // Still kneeling after the recenter: the view stays low.
    CHECK(relative(ps.on_head_pose(unscaled(raw_pose(0.0f, 1.0f, 0.0f)), floor).game, ref).y ==
          doctest::Approx(0.7f).epsilon(kTol));
    // Standing straight: the character's eye height.
    CHECK(std::abs(relative(ps.on_head_pose(unscaled(raw_pose(0.0f, 1.7f, 0.0f)), floor).game, ref).y) <
          kTol);
    // Ducking 30 cm lowers the view 30 cm.
    CHECK(relative(ps.on_head_pose(unscaled(raw_pose(0.0f, 1.4f, 0.0f)), floor).game, ref).y ==
          doctest::Approx(0.3f).epsilon(kTol));

    SUBCASE("the floor in the runtime's space counts (OpenXR's seated space)") {
        const Matrix34 r2 = ps.on_reference(unscaled(raw_pose(0.0f, 0.0f, 0.0f)), -1.7f, "OpenXR");
        CHECK(std::abs(relative(ps.on_head_pose(unscaled(raw_pose(0.0f, 0.0f, 0.0f)), -1.7f).game, r2).y) <
              kTol);
        CHECK(relative(ps.on_head_pose(unscaled(raw_pose(0.0f, -0.3f, 0.0f)), -1.7f).game, r2).y ==
              doctest::Approx(0.3f).epsilon(kTol));
    }
    SUBCASE("h is in runtime metres, before s") {
        ps.begin_frame(settings(PlayMode::Standing, 1.7f, 0.5f));
        const Matrix34 r2 = ps.on_reference(unscaled(raw_pose(0.0f, 1.0f, 0.0f)), floor, "OpenVR");
        CHECK(relative(ps.on_head_pose(unscaled(raw_pose(0.0f, 1.4f, 0.0f)), floor).game, r2).y ==
              doctest::Approx(0.5f * 0.3f).epsilon(kTol));
    }
    SUBCASE("the seated eye raise does not apply standing") {
        auto s               = settings(PlayMode::Standing, 1.7f);
        s.seated_eye_raise_m = 0.4f;
        ps.begin_frame(s);
        CHECK(std::abs(relative(ps.on_head_pose(unscaled(raw_pose(0.0f, 1.7f, 0.0f)), floor).game, ref).y) <
              kTol);
    }
    SUBCASE("the hands' head is the real one") {
        const HeadPoses p = ps.on_head_pose(unscaled(raw_pose(0.0f, 1.4f, 0.0f)), floor);
        CHECK(same_bits(p.tracked, to_game_pose(raw_pose(0.0f, 1.4f, 0.0f).m, 1.0f)));
        CHECK(same_bits(p.player, p.game)); // standing height is the player's own movement
    }
}

TEST_CASE("standing: not calibrated or no floor plays seated") {
    RecordingLog log;
    PlaySpace ps{log};
    SUBCASE("not calibrated") {
        ps.begin_frame(settings(PlayMode::Standing, 0.0f));
        const Matrix34 ref = ps.on_reference(unscaled(raw_pose(0.0f, 1.0f, 0.0f)), 0.0f, "OpenVR");
        CHECK(std::abs(relative(ps.on_head_pose(unscaled(raw_pose(0.0f, 1.0f, 0.0f)), 0.0f).game, ref).y) <
              kTol);
        CHECK(log.contains("not calibrated"));
        CHECK(ps.wants_automatic_calibration());
        REQUIRE(ps.reference_height());
        CHECK(*ps.reference_height() == doctest::Approx(1.0f));
    }
    SUBCASE("no floor reference: logged once") {
        ps.begin_frame(settings(PlayMode::Standing, 1.7f));
        const Matrix34 ref = ps.on_reference(unscaled(raw_pose(0.0f, 1.0f, 0.0f)), std::nullopt, "OpenXR");
        ps.on_reference(unscaled(raw_pose(0.0f, 1.0f, 0.0f)), std::nullopt, "OpenXR");
        CHECK(std::abs(
                  relative(ps.on_head_pose(unscaled(raw_pose(0.0f, 1.0f, 0.0f)), std::nullopt).game, ref).y) <
              kTol);
        int warnings = 0;
        for (const auto& l : log.lines)
            warnings += l.find("standing play unavailable on OpenXR") != std::string::npos;
        CHECK(warnings == 1);
        CHECK_FALSE(ps.reference_height());
    }
}

TEST_CASE("play mode: a change asks for a recenter and applies there") {
    RecordingLog log;
    PlaySpace ps{log};
    ps.begin_frame(settings(PlayMode::Seated));
    const Matrix34 ref = ps.on_reference(unscaled(raw_pose(0.0f, 1.0f, 0.0f)), 0.0f, "OpenVR");
    CHECK(ps.begin_frame(settings(PlayMode::Standing, 1.7f)));       // the change
    CHECK_FALSE(ps.begin_frame(settings(PlayMode::Standing, 1.7f))); // once
    CHECK(ps.applied_mode() == PlayMode::Seated);
    // Until the recenter, still seated: no jump mid-play.
    CHECK(std::abs(relative(ps.on_head_pose(unscaled(raw_pose(0.0f, 1.0f, 0.0f)), 0.0f).game, ref).y) < kTol);
    ps.on_reference(unscaled(raw_pose(0.0f, 1.0f, 0.0f)), 0.0f, "OpenVR");
    CHECK(ps.applied_mode() == PlayMode::Standing);
}

TEST_CASE("Body.EyeForward moves the eye along the reference's forward") {
    RecordingLog log;
    PlaySpace ps{log};
    auto s          = settings();
    s.eye_forward_m = 0.15f;
    ps.begin_frame(s);
    const float yaw    = std::numbers::pi_v<float> / 2.0f;
    const Matrix34 ref = ps.on_reference(unscaled(raw_pose(0.0f, 1.2f, 0.0f, yaw)), 0.0f, "OpenVR");
    const HeadPoses p  = ps.on_head_pose(unscaled(raw_pose(0.0f, 1.2f, 0.0f)), 0.0f);
    // OpenVR forward is -Z turned by the yaw: (-sin, 0, -cos). The head moved
    // there by 0.15 m, in the convention (negated).
    const Rel r = relative(p.game, ref);
    CHECK(r.x == doctest::Approx(0.15f).epsilon(kTol));
    CHECK(std::abs(r.y) < kTol);
    CHECK(std::abs(r.z) < kTol);
    CHECK(std::abs(relative(p.player, ref).x) < kTol);
    CHECK(same_bits(p.tracked, to_game_pose(raw_pose(0.0f, 1.2f, 0.0f).m, 1.0f)));
}

TEST_CASE("height calibration: 3 s, then the first valid pose in use, 1.0 to 2.2 m") {
    RecordingLog log;
    HeightCalibration cal{log};
    const Time t0{};
    const CalibrationReading worn{.has_floor = true, .in_use = true, .head_height_m = 1.72f};
    CHECK(cal.status(t0).state == CalibrationState::Idle);
    cal.request(t0);
    CHECK(cal.pending());
    CHECK(cal.status(t0).seconds_left == 3);
    CHECK(cal.status(t0 + 1500ms).seconds_left == 2);
    CHECK(cal.status(t0 + 2500ms).seconds_left == 1);
    CHECK_FALSE(cal.update(t0 + 2900ms, worn)); // still counting

    SUBCASE("taken from the first valid pose") {
        CHECK_FALSE(cal.update(t0 + 3s, {.has_floor = true, .in_use = false, .head_height_m = std::nullopt}));
        CHECK(cal.status(t0 + 3s).state == CalibrationState::Waiting);
        CHECK(log.contains("waiting"));
        const auto h = cal.update(t0 + 4s, worn);
        REQUIRE(h);
        CHECK(*h == 1.72f);
        CHECK(cal.status(t0 + 4s).state == CalibrationState::Taken);
        CHECK(log.contains("calibrated: 1.72 m"));
        CHECK_FALSE(cal.pending());
    }
    SUBCASE("outside the range: not taken") {
        CHECK_FALSE(cal.update(t0 + 3s, {.has_floor = true, .in_use = true, .head_height_m = 2.3f}));
        CHECK(cal.status(t0 + 3s).state == CalibrationState::OutOfRange);
        CHECK(cal.status(t0 + 3s).height_m == 2.3f);
        CHECK(log.contains("not taken: 2.30 m is outside 1.00 to 2.20 m"));
    }
    SUBCASE("no floor reference") {
        CHECK_FALSE(cal.update(t0 + 3s, {.has_floor = false, .in_use = true, .head_height_m = std::nullopt}));
        CHECK(cal.status(t0 + 3s).state == CalibrationState::NoFloor);
        CHECK(log.contains("no floor reference"));
    }
}

TEST_CASE("height calibration: the automatic first one checks the range too") {
    RecordingLog log;
    HeightCalibration cal{log};
    CHECK(cal.take_automatic(1.65f) == 1.65f);
    CHECK(log.contains("automatic"));
    CHECK_FALSE(cal.take_automatic(0.95f));
    CHECK(calibration_in_range(1.0f));
    CHECK(calibration_in_range(2.2f));
    CHECK_FALSE(calibration_in_range(0.99f));
    CHECK_FALSE(calibration_in_range(2.21f));
}
