// OpenMotherVR-Provenance: clean
// core/hands: the hand mapping (spec 21 §3.1), smoothing (§3.2), a lost
// controller (§3.3), when hands are active (§4.4), aiming from the hand
// (§5.2, §5.3) and the tracker's facing (§6.2). Expected values follow
// from the spec's formulas, computed here independently of the code.

#include "core/config/settings.h"
#include "core/hands/action_episode.h"
#include "core/hands/activity.h"
#include "core/hands/hand_aim.h"
#include "core/hands/hand_track.h"
#include "core/hands/hands.h"
#include "core/hands/motion_actions.h"
#include "core/hands/pose_filter.h"
#include "core/hands/tuning.h"
#include "core/vr/pose_convert.h"
#include "fakes/fakes.h"
#include "util/math/pose.h"
#include "util/math/quat.h"

#include <doctest/doctest.h>

#include <chrono>
#include <cmath>
#include <numbers>
#include <optional>

namespace {

using namespace omvr::core::hands;
using omvr::game::WeaponType;
using omvr::util::Matrix34;
using omvr::util::Quat;
using omvr::util::Vec3;
using Raw = float[3][4];

constexpr float kEps = 1e-4f;

// A runtime-convention pose (right-handed, +Y up, -Z forward): a rotation
// by `angle` about the unit `axis`, at `p`.
void raw_pose(Raw& out, Vec3 axis, float angle, Vec3 p) {
    const float s = std::sin(angle / 2);
    omvr::util::pose_matrix({{axis.x * s, axis.y * s, axis.z * s, std::cos(angle / 2)}, p}, out);
}

void copy(const Raw& in, Raw& out) {
    for (int r = 0; r < 3; ++r)
        for (int c = 0; c < 4; ++c) out[r][c] = in[r][c];
}

// 21 §3.1 written out on the runtime's poses, independently of the code:
//   d = Rh^T (pg - ph); d' = (d.x, d.y, -d.z) s; Pw = Pc + rotate(Qc, d')
//   Rrel = M Rh^T Rg M; Rw = Qc Rrel
// The rotation is checked through the image of a vector: Rw v.
struct SpecResult {
    Vec3 position;
    Vec3 rotated; // Rw applied to the test vector
};

SpecResult spec_mapping(const Raw& h, const Raw& g, float s, const CameraPose& cam, Vec3 v) {
    float d[3];
    for (int c = 0; c < 3; ++c) {
        d[c] = 0;
        for (int r = 0; r < 3; ++r) d[c] += h[r][c] * (g[r][3] - h[r][3]);
    }
    const Vec3 dp{d[0] * s, d[1] * s, -d[2] * s};
    // Rrel v = M Rh^T Rg M v.
    const float mv[3] = {v.x, v.y, -v.z};
    float rg[3], rhrg[3];
    for (int r = 0; r < 3; ++r) rg[r] = g[r][0] * mv[0] + g[r][1] * mv[1] + g[r][2] * mv[2];
    for (int c = 0; c < 3; ++c) rhrg[c] = h[0][c] * rg[0] + h[1][c] * rg[1] + h[2][c] * rg[2];
    const Vec3 rrel{rhrg[0], rhrg[1], -rhrg[2]};
    return {omvr::util::add(cam.position, omvr::util::rotate(cam.orientation, dp)),
            omvr::util::rotate(cam.orientation, rrel)};
}

void check_close(Vec3 a, Vec3 b) {
    CHECK(a.x == doctest::Approx(b.x).epsilon(kEps));
    CHECK(a.y == doctest::Approx(b.y).epsilon(kEps));
    CHECK(a.z == doctest::Approx(b.z).epsilon(kEps));
}

Quat yaw(float angle) { return {0, std::sin(angle / 2), 0, std::cos(angle / 2)}; }

// The code's result for runtime poses, through the game convention the
// stereo device hands out (to_game_pose).
WorldPose code_mapping(const Raw& h, const Raw& g, float s, const CameraPose& cam) {
    const Matrix34 head = omvr::core::vr::to_game_pose(h, s);
    const Matrix34 hand = omvr::core::vr::to_game_pose(g, s);
    return to_world(cam, head_local(head, hand));
}

TEST_CASE("hand mapping: a hand ahead of a level head is ahead of the camera") {
    Raw h, g;
    raw_pose(h, {0, 1, 0}, 0, {0, 1.6f, 0});
    raw_pose(g, {0, 1, 0}, 0, {0.2f, 1.4f, -0.5f});
    const CameraPose cam{};
    const WorldPose w = code_mapping(h, g, 1.0f, cam);
    // d = (0.2, -0.2, -0.5): right, below, ahead; d' = (0.2, -0.2, 0.5).
    check_close(w.position, {0.2f, -0.2f, 0.5f});
    // The grip's -Z (where the fist points) is the camera's +Z (ahead).
    check_close(pointing(w), {0, 0, 1});
}

TEST_CASE("hand mapping: only the hand relative to the head counts; scale and camera apply") {
    Raw h, g;
    // Head turned 90 degrees left (forward -Z becomes -X), hand 0.5 m in
    // front of it, also turned with it: straight ahead of the eye.
    const float quarter = std::numbers::pi_v<float> / 2;
    raw_pose(h, {0, 1, 0}, quarter, {1, 1.7f, 2});
    raw_pose(g, {0, 1, 0}, quarter, {0.5f, 1.7f, 2});
    const CameraPose cam{yaw(0.3f), {10, 2, -4}};
    const WorldPose w = code_mapping(h, g, 2.0f, cam);
    // d' = (0, 0, 0.5) * 2, along the camera's forward.
    check_close(w.position, omvr::util::add(cam.position, omvr::util::rotate(cam.orientation, {0, 0, 1})));
    check_close(pointing(w), omvr::util::rotate(cam.orientation, {0, 0, 1}));
}

TEST_CASE("hand mapping matches 21 §3.1 for general poses") {
    Raw h, g;
    const float n = 1 / std::sqrt(3.0f);
    raw_pose(h, {0.6f, 0.8f, 0}, 0.7f, {0.3f, 1.5f, -0.2f});
    raw_pose(g, {n, -n, n}, 1.9f, {-0.1f, 1.1f, -0.6f});
    const CameraPose cam{omvr::util::normalized(Quat{0.1f, 0.4f, -0.2f, 0.9f}), {3, 1, 7}};
    const float s     = 1.3f;
    const WorldPose w = code_mapping(h, g, s, cam);
    for (const Vec3 v : {Vec3{1, 0, 0}, Vec3{0, 1, 0}, Vec3{0, 0, 1}}) {
        const SpecResult e = spec_mapping(h, g, s, cam, v);
        check_close(w.position, e.position);
        check_close(omvr::util::rotate(w.orientation, v), e.rotated);
    }
}

TEST_CASE("quaternion of a rotation matrix, product, nlerp") {
    for (const float angle : {0.0f, 1.0f, 3.0f}) {
        Raw m;
        raw_pose(m, {0, 0, 1}, angle, {});
        Matrix34 mm{};
        copy(m, mm.m);
        const Quat q = omvr::util::quat_from_rotation(mm);
        CHECK(q.z == doctest::Approx(std::sin(angle / 2)).epsilon(kEps));
        CHECK(q.w == doctest::Approx(std::cos(angle / 2)).epsilon(kEps));
    }
    const Quat a = yaw(0.4f), b = yaw(0.2f);
    const Quat ab = omvr::util::multiply(a, b);
    CHECK(ab.y == doctest::Approx(std::sin(0.3f)));
    CHECK(omvr::util::angle_between(a, b) == doctest::Approx(0.2f).epsilon(kEps));
    const Quat half = omvr::util::nlerp(yaw(0), yaw(1.0f), 0.5f);
    CHECK(omvr::util::yaw_of(half) == doctest::Approx(0.5f).epsilon(kEps));
}

TEST_CASE("smoothing: 0 is off; the cut-off goes from 10 Hz to 1 Hz") {
    CHECK(smoothing_cutoff_hz(0.0f) == doctest::Approx(10.0f));
    CHECK(smoothing_cutoff_hz(1.0f) == doctest::Approx(1.0f));
    CHECK(smoothing_cutoff_hz(0.5f) == doctest::Approx(5.5f));
    // 1 / (1 + 1 / (2 pi fc dt)).
    CHECK(smoothing_alpha(10.0f, 1.0f / 90) ==
          doctest::Approx(1.0f / (1.0f + 1.0f / (2 * std::numbers::pi_v<float> * 10.0f / 90))));

    PoseFilter f;
    Raw a, b;
    raw_pose(a, {0, 1, 0}, 0, {0, 1, 0});
    raw_pose(b, {0, 1, 0}, 0, {0.01f, 1, 0});
    Matrix34 ma{}, mb{};
    copy(a, ma.m);
    copy(b, mb.m);
    // Off: every sample passes unchanged.
    CHECK(f.apply(ma, 1.0f / 90, 0.0f, 1.0f).m[0][3] == 0.0f);
    CHECK(f.apply(mb, 1.0f / 90, 0.0f, 1.0f).m[0][3] == 0.01f);

    // On: a 1 cm tremor at rest is damped; a still hand stays put.
    PoseFilter on;
    CHECK(on.apply(ma, 1.0f / 90, 1.0f, 1.0f).m[0][3] == 0.0f); // the first sample starts it
    const float moved = on.apply(mb, 1.0f / 90, 1.0f, 1.0f).m[0][3];
    CHECK(moved > 0.0f);
    CHECK(moved < 0.01f / 2);
    PoseFilter still;
    for (int i = 0; i < 10; ++i)
        CHECK(still.apply(ma, 1.0f / 90, 1.0f, 1.0f).m[1][3] == doctest::Approx(1.0f));
}

TEST_CASE("a lost hand is held 0.3 s, blends out over 0.3 s and back in over 0.15 s") {
    const HeadLocalPose p1{{0.1f, 0, 0.4f}, {}};
    const HeadLocalPose p2{{0.2f, 0, 0.4f}, {}};
    HandTrack t;
    CHECK(t.update(Seconds{0}, p1, p1) == HandTrack::Change::Regained);
    CHECK(t.weight() == 0.0f);
    t.update(Seconds{0.075f}, p1, p1);
    CHECK(t.weight() == doctest::Approx(0.5f));
    t.update(Seconds{0.1f}, p1, p1);
    CHECK(t.weight() == 1.0f);

    CHECK(t.update(Seconds{0.1f}, std::nullopt, std::nullopt) == HandTrack::Change::Lost);
    t.update(Seconds{0.15f}, std::nullopt, std::nullopt); // 0.25 s: held
    CHECK(t.weight() == 1.0f);
    CHECK(t.grip().offset.x == doctest::Approx(0.1f));   // the last pose relative to the head
    t.update(Seconds{0.2f}, std::nullopt, std::nullopt); // 0.45 s: half way out
    CHECK(t.weight() == doctest::Approx(0.5f));
    t.update(Seconds{0.2f}, std::nullopt, std::nullopt); // 0.65 s: out
    CHECK(t.weight() == 0.0f);

    CHECK(t.update(Seconds{0.075f}, p2, p2) == HandTrack::Change::Regained);
    CHECK(t.weight() == doctest::Approx(0.5f));
    CHECK(t.grip().offset.x == doctest::Approx(0.2f));
}

TEST_CASE("hands are active only in first-person play the player controls (21 §4.4)") {
    ActivityInput in{.enabled          = true,
                     .stereo_running   = true,
                     .world_mode       = true,
                     .player           = true,
                     .alive            = true,
                     .follow_active    = true,
                     .cutscene_or_fade = false,
                     .menu_screen      = false,
                     .interaction      = false,
                     .hiding           = false};
    CHECK(activity(in) == Activity::Active);
    auto with = [&](auto change, Activity expected) {
        ActivityInput c = in;
        change(c);
        CHECK(activity(c) == expected);
    };
    with([](ActivityInput& c) { c.enabled = false; }, Activity::Off);
    with([](ActivityInput& c) { c.stereo_running = false; }, Activity::NoStereo);
    with([](ActivityInput& c) { c.world_mode = false; }, Activity::Screen);
    with([](ActivityInput& c) { c.player = false; }, Activity::NoPlayer);
    with([](ActivityInput& c) { c.alive = false; }, Activity::Dead);
    with([](ActivityInput& c) { c.follow_active = false; }, Activity::CameraTaken);
    with([](ActivityInput& c) { c.cutscene_or_fade = true; }, Activity::Cutscene);
    with([](ActivityInput& c) { c.menu_screen = true; }, Activity::MenuScreen);
    with([](ActivityInput& c) { c.interaction = true; }, Activity::Interaction);
    with([](ActivityInput& c) { c.hiding = true; }, Activity::Hiding);
    with([](ActivityInput& c) { c.player_action = true; }, Activity::PlayerAction);
    CHECK(is_episode(Activity::PlayerAction));
    CHECK(is_episode(Activity::Cutscene));
    CHECK_FALSE(is_episode(Activity::Dead));
}

TEST_CASE("weapon classes of 21 §5.2") {
    CHECK(weapon_class(std::nullopt) == WeaponClass::NoWeapon);
    CHECK(weapon_class(WeaponType::NoWeapon) == WeaponClass::NoWeapon);
    CHECK(weapon_class(WeaponType::Pistol) == WeaponClass::Barrel);
    CHECK(weapon_class(WeaponType::Shotgun) == WeaponClass::Barrel);
    CHECK(weapon_class(WeaponType::Boltgun) == WeaponClass::Barrel);
    CHECK(weapon_class(WeaponType::Flamethrower) == WeaponClass::Nozzle);
    CHECK(weapon_class(WeaponType::Melee) == WeaponClass::Contact);
    CHECK(weapon_class(WeaponType::Cattleprod) == WeaponClass::Contact);
    CHECK(weapon_class(WeaponType::Unknown) == WeaponClass::Unknown);
    CHECK(aims_from_hand(WeaponClass::Barrel));
    CHECK(aims_from_hand(WeaponClass::Nozzle));
    CHECK(aims_from_hand(WeaponClass::NoWeapon));
    CHECK_FALSE(aims_from_hand(WeaponClass::Contact));
    CHECK_FALSE(aims_from_hand(WeaponClass::Unknown));
}

TEST_CASE("the hand's aim ray keeps the game's range; weight blends from the head's") {
    // The game's ray: from the eye along +Z, 50 m.
    const AimRay game{{0, 1.6f, 0}, {0, 0, 1}, {0, 1.6f, 50}};
    // The hand at (0.3, 1.2, 0.4) pointing along world +X.
    const WorldPose hand{{0.3f, 1.2f, 0.4f}, yaw(std::numbers::pi_v<float> / 2)};
    const AimRay r = hand_aim_ray(game, hand, 1.0f, 0.0f);
    check_close(r.start, hand.position);
    check_close(r.direction, {1, 0, 0});
    check_close(r.end, {50.3f, 1.2f, 0.4f});

    const AimRay off = hand_aim_ray(game, hand, 0.0f, 0.0f);
    check_close(off.start, game.start);
    check_close(off.direction, game.direction);
    check_close(off.end, game.end);

    const AimRay half = hand_aim_ray(game, hand, 0.5f, 0.0f);
    CHECK(omvr::util::length(half.direction) == doctest::Approx(1.0f));
    CHECK(omvr::util::length(omvr::util::sub(half.end, half.start)) == doctest::Approx(50.0f));

    const AimRay ahead = hand_aim_ray(game, hand, 1.0f, 0.25f);
    check_close(ahead.start, {0.55f, 1.2f, 0.4f});

    CHECK(fire_gate_threshold(true, 0.75f) < 0.0f);
    CHECK(fire_gate_threshold(false, 0.75f) == 0.75f);
}

TEST_CASE("tracker facing: the hand's horizontal direction; nearly vertical keeps the last") {
    TrackerFacing t;
    CHECK_FALSE(t.update({0, 1, 0}).has_value()); // straight up before any facing
    const auto f = t.update({3, 1, 4});
    REQUIRE(f.has_value());
    CHECK(f->x == doctest::Approx(0.6f));
    CHECK(f->y == doctest::Approx(0.8f)); // world z
    const auto kept = t.update({0.05f, -1, 0.05f});
    REQUIRE(kept.has_value());
    CHECK(kept->x == doctest::Approx(0.6f));
    t.reset();
    CHECK_FALSE(t.update({0, -1, 0}).has_value());
}

// The whole path: settings, a frame, the published view, the aim and the
// tracker, with a recording log.
struct HandsRig {
    omvr::core::config::HandsSettings settings{};
    omvr::tests::RecordingLog log;
    Hands hands{settings, log};
    TimePoint now{std::chrono::seconds(100)};

    static ActivityInput active() {
        return {.enabled          = true,
                .stereo_running   = true,
                .world_mode       = true,
                .player           = true,
                .alive            = true,
                .follow_active    = true,
                .cutscene_or_fade = false,
                .menu_screen      = false,
                .interaction      = false,
                .hiding           = false};
    }

    // Head level at the origin; both hands 0.5 m ahead, pointing ahead.
    static HandFrame frame() {
        Raw h, g;
        raw_pose(h, {0, 1, 0}, 0, {0, 0, 0});
        raw_pose(g, {0, 1, 0}, 0, {0, 0, -0.5f});
        HandFrame f;
        f.head = omvr::core::vr::to_game_pose(h, 1.0f);
        for (auto& s : f.hands) {
            s.grip       = omvr::core::vr::to_game_pose(g, 1.0f);
            s.aim        = s.grip;
            s.grip_valid = s.aim_valid = true;
        }
        return f;
    }

    void run(std::chrono::milliseconds step, int frames, const ActivityInput& in) {
        for (int i = 0; i < frames; ++i) {
            now += step;
            (void)hands.update(frame(), in, {}, now, 1.0f);
        }
    }
};

TEST_CASE("Hands: off by default; on, the right hand aims and the left faces the tracker") {
    HandsRig rig;
    const AimRay game{{0, 0, 0}, {0, 0, 1}, {0, 0, 20}};
    const CameraPose cam{yaw(std::numbers::pi_v<float> / 2), {5, 0, 5}}; // looking along world +X

    // Hands.Enabled off (the default): nothing at all.
    rig.run(std::chrono::milliseconds(11), 5, {});
    CHECK_FALSE(rig.hands.aim(game, cam, {WeaponType::Pistol}, rig.now).ray.has_value());
    CHECK_FALSE(rig.hands.tracker_facing(cam, std::nullopt, rig.now).has_value());
    CHECK(rig.log.lines.empty());

    rig.settings.enabled = true;
    rig.run(std::chrono::milliseconds(11), 30, HandsRig::active());
    CHECK(rig.log.contains("hands: tracked hands active"));
    const auto r = rig.hands.aim(game, cam, {WeaponType::Pistol}, rig.now);
    REQUIRE(r.ray.has_value());
    CHECK(r.hand_aiming);
    check_close(r.ray->start, {5.5f, 0, 5});
    check_close(r.ray->direction, {1, 0, 0});
    check_close(r.ray->end, {25.5f, 0, 5});

    // Contact weapons keep the game's ray; so does Aim from Hand off.
    CHECK_FALSE(rig.hands.aim(game, cam, {WeaponType::Melee}, rig.now).ray.has_value());
    rig.settings.aim_from_hand = false;
    CHECK_FALSE(rig.hands.aim(game, cam, {WeaponType::Pistol}, rig.now).ray.has_value());
    rig.settings.aim_from_hand = true;

    const auto facing = rig.hands.tracker_facing(cam, std::nullopt, rig.now);
    REQUIRE(facing.has_value());
    CHECK(facing->x == doctest::Approx(1.0f));
    rig.settings.tracker_in_hand = false;
    CHECK_FALSE(rig.hands.tracker_facing(cam, std::nullopt, rig.now).has_value());
    rig.settings.tracker_in_hand = true;

    // No update for a while (a menu): stale, the game's ray.
    CHECK_FALSE(
        rig.hands.aim(game, cam, {WeaponType::Pistol}, rig.now + std::chrono::seconds(1)).ray.has_value());
}

TEST_CASE("Hands: a cutscene hands the aim back to the head over the blend") {
    HandsRig rig;
    rig.settings.enabled = true;
    rig.run(std::chrono::milliseconds(10), 30, HandsRig::active());
    ActivityInput cut    = HandsRig::active();
    cut.cutscene_or_fade = true;
    rig.run(std::chrono::milliseconds(10), 40, cut); // past kBlendOut
    const AimRay game{{0, 0, 0}, {0, 0, 1}, {0, 0, 20}};
    CHECK_FALSE(rig.hands.aim(game, CameraPose{}, {WeaponType::Pistol}, rig.now).ray.has_value());
    // Episodes are logged only with debug options on.
    CHECK_FALSE(rig.log.contains("cutscene"));
}

// A weapon transform turned 90 degrees about +Y (its local +Z, the barrel
// column, along world +X) at (2, 1, 3): row-major, translation in the
// fourth column, as WeaponTransform fills it (21 §2.5).
Matrix34 weapon_along_x() { return Matrix34{{{0, 0, 1, 2}, {0, 1, 0, 1}, {-1, 0, 0, 3}}}; }

TEST_CASE("aim source: the revolver, shotgun and flamethrower from the drawn weapon; the rest the aim pose") {
    CHECK(aim_source(WeaponType::Pistol, true) == AimSource::WeaponTransform);
    CHECK(aim_source(WeaponType::Shotgun, true) == AimSource::WeaponTransform);
    CHECK(aim_source(WeaponType::Flamethrower, true) == AimSource::WeaponTransform);
    CHECK(aim_source(WeaponType::Boltgun, true) == AimSource::AimPose); // compared in game first
    CHECK(aim_source(std::nullopt, true) == AimSource::AimPose);        // throwables
    CHECK(aim_source(WeaponType::NoWeapon, true) == AimSource::AimPose);
    // Not drawn in the moved hand: the game's animation placed it.
    CHECK(aim_source(WeaponType::Pistol, false) == AimSource::AimPose);
}

TEST_CASE("weapon barrel: origin and barrel column; identity, NaN and degenerate are not used") {
    const auto line = weapon_barrel(weapon_along_x(), 0.0f);
    REQUIRE(line.has_value());
    check_close(line->start, {2, 1, 3});
    check_close(line->direction, {1, 0, 0});
    const auto ahead = weapon_barrel(weapon_along_x(), 0.5f);
    REQUIRE(ahead.has_value());
    check_close(ahead->start, {2.5f, 1, 3});

    CHECK_FALSE(weapon_barrel(Matrix34{{{1, 0, 0, 0}, {0, 1, 0, 0}, {0, 0, 1, 0}}}, 0.0f).has_value());
    Matrix34 nan = weapon_along_x();
    nan.m[1][3]  = std::nanf("");
    CHECK_FALSE(weapon_barrel(nan, 0.0f).has_value());
    CHECK_FALSE(weapon_barrel(Matrix34{{{1, 0, 0, 1}, {0, 1, 0, 1}, {0, 0, 0, 1}}}, 0.0f).has_value());
}

TEST_CASE("Hands: a weapon drawn in the moved hand aims along its barrel; otherwise the aim pose") {
    HandsRig rig;
    rig.settings.enabled = true;
    rig.run(std::chrono::milliseconds(11), 30, HandsRig::active());
    const AimRay game{{0, 0, 0}, {0, 0, 1}, {0, 0, 20}};
    const CameraPose cam{yaw(std::numbers::pi_v<float> / 2), {5, 0, 5}};

    const auto barrel = rig.hands.aim(game, cam, {WeaponType::Shotgun, weapon_along_x(), true}, rig.now);
    REQUIRE(barrel.ray.has_value());
    CHECK(barrel.hand_aiming);
    check_close(barrel.ray->start, {2, 1, 3});
    check_close(barrel.ray->direction, {1, 0, 0});
    check_close(barrel.ray->end, {22, 1, 3});

    // The bolt gun, a weapon the game's animation placed, and a transform
    // that cannot be used all aim from the aim pose: 0.5 m ahead of the
    // camera at (5, 0, 5), along world +X.
    for (const WeaponSample& w :
         {WeaponSample{WeaponType::Boltgun, weapon_along_x(), true},
          WeaponSample{WeaponType::Pistol, weapon_along_x(), false},
          WeaponSample{WeaponType::Pistol, Matrix34{{{1, 0, 0, 0}, {0, 1, 0, 0}, {0, 0, 1, 0}}}, true},
          WeaponSample{WeaponType::Flamethrower, std::nullopt, true}}) {
        const auto r = rig.hands.aim(game, cam, w, rig.now);
        REQUIRE(r.ray.has_value());
        check_close(r.ray->start, {5.5f, 0, 5});
        check_close(r.ray->direction, {1, 0, 0});
    }
}

TEST_CASE("Hands: the weapon transform is read where it aims, or once per type with debug options") {
    HandsRig rig;
    rig.settings.enabled = true;
    rig.run(std::chrono::milliseconds(11), 30, HandsRig::active());
    const AimRay game{{0, 0, 0}, {0, 0, 1}, {0, 0, 20}};
    const CameraPose cam{yaw(std::numbers::pi_v<float> / 2), {5, 0, 5}};

    CHECK(rig.hands.weapon_transform_wanted(WeaponType::Pistol, true, rig.now));
    CHECK_FALSE(rig.hands.weapon_transform_wanted(WeaponType::Pistol, false, rig.now));
    CHECK_FALSE(rig.hands.weapon_transform_wanted(WeaponType::Melee, true, rig.now));
    CHECK_FALSE(rig.hands.weapon_transform_wanted(WeaponType::Boltgun, false, rig.now));

    rig.log.debug = true;
    CHECK(rig.hands.weapon_transform_wanted(WeaponType::Pistol, false, rig.now));
    (void)rig.hands.aim(game, cam, {WeaponType::Pistol, weapon_along_x(), false}, rig.now);
    CHECK(rig.log.contains(
        "[diag] hands: weapon transform, weapon type 2: columns (+0.000 +0.000 -1.000) "
        "(+0.000 +1.000 +0.000) (+1.000 +0.000 +0.000), origin (2.000 1.000 3.000); aim pose "
        "forward (+1.000"));
    CHECK(rig.log.contains("drawn by the game's animation"));
    CHECK_FALSE(rig.hands.weapon_transform_wanted(WeaponType::Pistol, false, rig.now)); // once per type
    CHECK(rig.hands.weapon_transform_wanted(WeaponType::Shotgun, false, rig.now));
}

TEST_CASE("Hands: the tracker faces by the drawn left hand when given, else its aim pose") {
    HandsRig rig;
    rig.settings.enabled = true;
    rig.run(std::chrono::milliseconds(11), 30, HandsRig::active());
    const CameraPose cam{yaw(std::numbers::pi_v<float> / 2), {5, 0, 5}};
    const auto drawn = rig.hands.tracker_facing(cam, Vec3{0, 0.5f, -2}, rig.now);
    REQUIRE(drawn.has_value());
    CHECK(drawn->x == doctest::Approx(0.0f));
    CHECK(drawn->y == doctest::Approx(-1.0f));
    const auto aim = rig.hands.tracker_facing(cam, std::nullopt, rig.now);
    REQUIRE(aim.has_value());
    CHECK(aim->x == doctest::Approx(1.0f));
}

// ---- Melee and throwing by motion (21 §8.2, §9.2), player actions (§12.1) ----

using std::chrono::milliseconds;

TimePoint at_ms(int ms) { return TimePoint{std::chrono::seconds(100)} + milliseconds(ms); }

TEST_CASE("swing: above 2 m/s, then below 1 m/s within 0.5 s") {
    SwingDetector d;
    CHECK_FALSE(d.update(0.5f, at_ms(0)));
    CHECK_FALSE(d.update(2.5f, at_ms(10)));
    CHECK_FALSE(d.update(1.5f, at_ms(200))); // slowing, not yet below 1
    CHECK(d.update(0.8f, at_ms(400)));       // the swing
    CHECK_FALSE(d.update(0.5f, at_ms(410)));

    // A slow movement is no swing.
    CHECK_FALSE(d.update(1.9f, at_ms(1000)));
    CHECK_FALSE(d.update(0.5f, at_ms(1100)));

    // Fast for longer than 0.5 s is no swing, and nothing counts until the
    // hand has slowed below 1 m/s.
    CHECK_FALSE(d.update(2.5f, at_ms(2000)));
    CHECK_FALSE(d.update(2.5f, at_ms(2600)));
    CHECK_FALSE(d.update(0.8f, at_ms(2650))); // the rest it waited for
    CHECK_FALSE(d.update(2.5f, at_ms(2700)));
    CHECK(d.update(0.5f, at_ms(2900)));

    // Lost velocity drops a swing in progress.
    CHECK_FALSE(d.update(2.5f, at_ms(4000)));
    CHECK_FALSE(d.update(std::nullopt, at_ms(4010)));
    CHECK_FALSE(d.update(0.5f, at_ms(4020)));
}

TEST_CASE("melee by motion: one 100 ms fire press per swing, none within 0.6 s of a press") {
    MeleeByMotion m;
    const auto swing = [&m](int start_ms, bool allowed = true) {
        (void)m.update(allowed, 3.0f, at_ms(start_ms));
        return m.update(allowed, 0.5f, at_ms(start_ms + 100));
    };
    const auto first = swing(0);
    REQUIRE(first.has_value());
    CHECK(*first == at_ms(200)); // pressed from 100 ms for 100 ms
    CHECK(m.pressed_shortly_before(at_ms(300)));
    CHECK_FALSE(swing(400).has_value()); // ends at 500 ms: within 0.6 s of the press at 100 ms
    CHECK(swing(700).has_value());       // ends at 800 ms
    CHECK_FALSE(swing(2000, false).has_value());
}

TEST_CASE("arm hand-off: out over 0.05 s, out until 0.9 s, back over 0.15 s; a restart keeps the arm out") {
    ArmHandOff a;
    CHECK(a.weight(at_ms(0)) == 1.0f);
    CHECK_FALSE(a.running(at_ms(0)));
    a.start(at_ms(0));
    CHECK(a.weight(at_ms(0)) == doctest::Approx(1.0f));
    CHECK(a.weight(at_ms(25)) == doctest::Approx(0.5f));
    CHECK(a.weight(at_ms(50)) == doctest::Approx(0.0f));
    CHECK(a.weight(at_ms(900)) == doctest::Approx(0.0f));
    CHECK(a.weight(at_ms(975)) == doctest::Approx(0.5f));
    CHECK(a.running(at_ms(975)));
    CHECK(a.weight(at_ms(1050)) == doctest::Approx(1.0f));
    CHECK_FALSE(a.running(at_ms(1050)));

    // A new start half way back: the arm goes out again from there, and
    // the 0.9 s run from the new start.
    a.start(at_ms(0));
    a.start(at_ms(975));
    CHECK(a.weight(at_ms(975)) == doctest::Approx(0.5f));
    CHECK(a.weight(at_ms(1000)) == doctest::Approx(0.0f));
    CHECK(a.weight(at_ms(1875)) == doctest::Approx(0.0f));
    CHECK(a.weight(at_ms(1950)) == doctest::Approx(0.5f));
}

TEST_CASE(
    "throw by motion: a release at 1.5 m/s or more throws; slower cancels; the peak of the last 50 ms") {
    ThrowByMotion t;
    const Vec3 fast{0, 0, 2.0f};
    const Vec3 slow{0, 0, 0.5f};
    CHECK_FALSE(t.update(true, false, fast, 1.0f, at_ms(0)).has_value()); // not armed
    CHECK_FALSE(t.update(true, true, slow, 1.0f, at_ms(10)).has_value());
    CHECK_FALSE(t.update(true, true, fast, 1.0f, at_ms(20)).has_value());
    const auto thrown = t.update(true, false, slow, 1.0f, at_ms(50));
    REQUIRE(thrown.has_value());
    CHECK(thrown->thrown);
    CHECK(thrown->speed_mps == doctest::Approx(2.0f));
    CHECK(thrown->local.z == doctest::Approx(2.0f));
    CHECK(thrown->at == at_ms(50));

    // The fast sample is older than 50 ms at the release: cancelled.
    (void)t.update(true, true, fast, 1.0f, at_ms(1000));
    (void)t.update(true, true, slow, 1.0f, at_ms(1040));
    const auto late = t.update(true, false, slow, 1.0f, at_ms(1060));
    REQUIRE(late.has_value());
    CHECK_FALSE(late->thrown);
    CHECK(late->speed_mps == doctest::Approx(0.5f));

    // Speeds are in metres: at world scale 2 a velocity of 2.8 game units
    // per second is 1.4 m/s.
    (void)t.update(true, true, Vec3{0, 0, 2.8f}, 2.0f, at_ms(2000));
    const auto scaled = t.update(true, false, std::nullopt, 2.0f, at_ms(2010));
    REQUIRE(scaled.has_value());
    CHECK_FALSE(scaled->thrown);

    // Not allowed (another item, hands inactive) disarms without a release.
    (void)t.update(true, true, fast, 1.0f, at_ms(3000));
    CHECK_FALSE(t.update(false, true, fast, 1.0f, at_ms(3010)).has_value());
    CHECK_FALSE(t.update(true, false, fast, 1.0f, at_ms(3020)).has_value());
}

TEST_CASE("throw velocity: rotated into the world, horizontal part times the strength, vertical kept") {
    const CameraPose cam{yaw(std::numbers::pi_v<float> / 2), {5, 0, 5}}; // looking along world +X
    check_close(throw_velocity(cam, {0, 1, 2}, 1.0f), {2, 1, 0});
    check_close(throw_velocity(cam, {0, 1, 2}, 2.0f), {4, 1, 0});
    check_close(throw_velocity(cam, {0, -1, 2}, 0.5f), {1, -1, 0});
}

TEST_CASE("player action episode: ends on an item taken up, the camera back 0.5 s, or after 5 s") {
    ActionEpisode e;
    CHECK_FALSE(e.pending_or_running());
    e.on_start(PlayerAction::TrapVictim, at_ms(0));
    CHECK(e.pending_or_running());
    auto c = e.update(5, true, at_ms(10)); // holding item 5 as it starts
    REQUIRE(c.started.has_value());
    CHECK(*c.started == PlayerAction::TrapVictim);
    CHECK(e.running());
    CHECK_FALSE(e.update(0, true, at_ms(20)).ended.has_value()); // put away: not an end
    c = e.update(7, true, at_ms(30));                            // a new item taken up
    REQUIRE(c.ended.has_value());
    CHECK(*c.ended == EpisodeEnd::ItemTaken);
    CHECK_FALSE(e.running());

    // The camera taken and given back: 0.5 s after it is back.
    e.on_start(PlayerAction::MeleeKill, at_ms(1000));
    (void)e.update(0, true, at_ms(1000));
    CHECK_FALSE(e.update(0, false, at_ms(1100)).ended.has_value());
    CHECK_FALSE(e.update(0, true, at_ms(1200)).ended.has_value());
    CHECK_FALSE(e.update(0, true, at_ms(1600)).ended.has_value());
    c = e.update(0, true, at_ms(1700));
    REQUIRE(c.ended.has_value());
    CHECK(*c.ended == EpisodeEnd::CameraBack);

    // Nothing else: 5 s after the start.
    e.on_start(PlayerAction::ImpactStanding, at_ms(2000));
    (void)e.update(3, true, at_ms(2000));
    CHECK_FALSE(e.update(3, true, at_ms(6900)).ended.has_value());
    c = e.update(3, true, at_ms(7000));
    REQUIRE(c.ended.has_value());
    CHECK(*c.ended == EpisodeEnd::Safety);
    CHECK(action_name(PlayerAction::MeleeIntercept) == "ACT_PLAYER_MELEE_INTERCEPT");
}

// HandsRig's frame with the right grip moving at `speed` m/s (world scale 1).
HandFrame moving_frame(float speed) {
    HandFrame f                        = HandsRig::frame();
    f.hands[kRightHand].velocity       = {0, 0, speed};
    f.hands[kRightHand].velocity_valid = true;
    return f;
}

TEST_CASE("Hands: a swing with the melee tool presses fire; a throwable's fast release presses fire") {
    HandsRig rig;
    rig.settings.enabled = true;
    rig.log.debug        = true;
    rig.run(milliseconds(11), 30, HandsRig::active());
    const PlayInput melee{true, WeaponType::Melee, false};

    rig.now += milliseconds(11);
    CHECK_FALSE(
        rig.hands.update(moving_frame(3.0f), HandsRig::active(), melee, rig.now, 1.0f).press_fire_until);
    rig.now += milliseconds(100);
    const auto out = rig.hands.update(moving_frame(0.5f), HandsRig::active(), melee, rig.now, 1.0f);
    REQUIRE(out.press_fire_until.has_value());
    CHECK(*out.press_fire_until == rig.now + milliseconds(100));

    // The same swing with the revolver, or with the item not read, does not.
    const PlayInput pistol{true, WeaponType::Pistol, false};
    rig.now += milliseconds(1000);
    (void)rig.hands.update(moving_frame(3.0f), HandsRig::active(), pistol, rig.now, 1.0f);
    rig.now += milliseconds(100);
    CHECK_FALSE(
        rig.hands.update(moving_frame(0.5f), HandsRig::active(), pistol, rig.now, 1.0f).press_fire_until);

    // A throwable (no weapon object): the Aim control held, let go fast.
    const PlayInput held{true, std::nullopt, true};
    const PlayInput let_go{true, std::nullopt, false};
    rig.now += milliseconds(11);
    (void)rig.hands.update(moving_frame(2.0f), HandsRig::active(), held, rig.now, 1.0f);
    rig.now += milliseconds(11);
    const auto thrown = rig.hands.update(moving_frame(1.8f), HandsRig::active(), let_go, rig.now, 1.0f);
    REQUIRE(thrown.press_fire_until.has_value());
    CHECK(rig.log.contains("[diag] hands: throw by motion: release speed 2.00 m/s, thrown"));
    REQUIRE(rig.hands.recent_throw(rig.now).has_value());
    CHECK_FALSE(rig.hands.recent_throw(rig.now + std::chrono::seconds(2)).has_value());

    // Let go slowly: cancelled, no press.
    rig.now += milliseconds(11);
    (void)rig.hands.update(moving_frame(0.3f), HandsRig::active(), held, rig.now, 1.0f);
    rig.now += milliseconds(11);
    CHECK_FALSE(
        rig.hands.update(moving_frame(0.3f), HandsRig::active(), let_go, rig.now, 1.0f).press_fire_until);
    CHECK(rig.log.contains("cancelled"));

    // Hands.MeleeByMotion off: no press.
    rig.settings.melee_by_motion = false;
    rig.now += milliseconds(1000);
    (void)rig.hands.update(moving_frame(3.0f), HandsRig::active(), melee, rig.now, 1.0f);
    rig.now += milliseconds(100);
    CHECK_FALSE(
        rig.hands.update(moving_frame(0.5f), HandsRig::active(), melee, rig.now, 1.0f).press_fire_until);
}

TEST_CASE("Hands: a melee start hands the right arm to the game; the left hand stays") {
    HandsRig rig;
    rig.settings.enabled = true;
    rig.log.debug        = true;
    rig.run(milliseconds(10), 30, HandsRig::active());
    const AimRay game{{0, 0, 0}, {0, 0, 1}, {0, 0, 20}};
    const CameraPose cam{yaw(std::numbers::pi_v<float> / 2), {5, 0, 5}};
    REQUIRE(rig.hands.aim(game, cam, {std::nullopt}, rig.now).ray.has_value());

    rig.hands.on_melee_start(rig.now);
    rig.run(milliseconds(10), 10, HandsRig::active()); // 0.1 s: out
    CHECK(rig.log.contains(
        "[diag] hands: melee: the right arm plays the game's swing (started by a motion: no)"));
    CHECK_FALSE(rig.hands.aim(game, cam, {std::nullopt}, rig.now).ray.has_value());
    CHECK(rig.hands.tracker_facing(cam, std::nullopt, rig.now).has_value());

    rig.run(milliseconds(10), 100, HandsRig::active()); // past 0.9 s + 0.15 s
    CHECK(rig.log.contains("[diag] hands: melee: the right arm is back on the controller"));
    const auto back = rig.hands.aim(game, cam, {std::nullopt}, rig.now);
    REQUIRE(back.ray.has_value());
    check_close(back.ray->direction, {1, 0, 0});
}

TEST_CASE("Hands: a player action makes an episode until it ends, logged with debug options") {
    HandsRig rig;
    rig.log.debug = true;
    CHECK_FALSE(rig.hands.player_action_wanted());
    rig.hands.on_player_action(PlayerAction::TrapVictim, rig.now);
    CHECK(rig.hands.player_action_wanted());
    CHECK(rig.hands.player_action(0, true, rig.now));
    CHECK(rig.log.contains("[diag] hands: ACT_PLAYER_TRAP_VICTIM started"));
    CHECK_FALSE(rig.hands.player_action(9, true, rig.now + milliseconds(100)));
    CHECK(rig.log.contains("[diag] hands: player action episode (player action) ended: an item taken up"));
    CHECK_FALSE(rig.hands.player_action_wanted());
}

} // namespace
