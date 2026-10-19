// OpenMotherVR-Provenance: clean
// core/hands: the hand mapping (spec 21 §3.1), smoothing (§3.2), a lost
// controller (§3.3), when hands are active (§4.4), aiming from the hand
// (§5.2, §5.3), the tracker's facing (§6.2) and the arm pose edit (§3.5,
// §4.2). Expected values follow
// from the spec's formulas, computed here independently of the code.

#include "core/config/settings.h"
#include "core/hands/action_episode.h"
#include "core/hands/activity.h"
#include "core/hands/arm_pose.h"
#include "core/hands/arm_poser.h"
#include "core/hands/hand_aim.h"
#include "core/hands/hand_track.h"
#include "core/hands/hands.h"
#include "core/hands/motion_actions.h"
#include "core/hands/pose_filter.h"
#include "core/hands/tuning.h"
#include "core/vr/pose_convert.h"
#include "fakes/fakes.h"
#include "game/hands/skeleton_names.h"
#include "util/math/pose.h"
#include "util/math/quat.h"

#include <doctest/doctest.h>

#include <array>
#include <chrono>
#include <cmath>
#include <numbers>
#include <optional>
#include <utility>

namespace {

using namespace omvr::core::hands;
using omvr::game::WeaponType;
namespace pose_flag = omvr::game::pose_flag;
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

// 21 §16: a grip tracked while its aim pose reads inactive aims from the
// game's head ray; the aim is held only while the whole hand is lost.
TEST_CASE("a tracked grip without an aim has no aim; a lost hand keeps its last aim") {
    const HeadLocalPose p1{{0.1f, 0, 0.4f}, {}};
    const HeadLocalPose a1{{0.1f, 0.05f, 0.5f}, {}};
    HandTrack t;
    t.update(Seconds{0}, p1, a1);
    REQUIRE(t.has_aim());
    CHECK(t.aim().offset.y == doctest::Approx(0.05f));

    t.update(Seconds{0.1f}, std::nullopt, std::nullopt); // lost: held
    CHECK(t.has_aim());
    CHECK(t.aim().offset.y == doctest::Approx(0.05f));

    t.update(Seconds{0.1f}, p1, std::nullopt); // the grip back, the aim not
    CHECK(t.has_grip());
    CHECK_FALSE(t.has_aim());

    t.update(Seconds{0.1f}, p1, a1); // the aim back
    CHECK(t.has_aim());
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

// tuning.h kMaxUpdateGap: a gap longer than 0.25 s (a pause menu) restarts
// the filters, so the first sample after it is taken as it is.
TEST_CASE("Hands: a long update gap restarts the smoothing filter") {
    HandsRig rig;
    rig.settings.enabled   = true;
    rig.settings.smoothing = 1.0f;
    rig.run(std::chrono::milliseconds(11), 30, HandsRig::active());
    const AimRay game{{0, 0, 0}, {0, 0, 1}, {0, 0, 20}};
    const CameraPose cam{yaw(std::numbers::pi_v<float> / 2), {5, 0, 5}}; // looking along world +X
    auto r = rig.hands.aim(game, cam, {WeaponType::Pistol}, rig.now);
    REQUIRE(r.ray.has_value());
    check_close(r.ray->start, {5.5f, 0, 5});

    // Two seconds later the hands are 0.3 m ahead instead of 0.5 m.
    Raw g;
    raw_pose(g, {0, 1, 0}, 0, {0, 0, -0.3f});
    HandFrame moved = HandsRig::frame();
    for (auto& h : moved.hands) h.grip = h.aim = omvr::core::vr::to_game_pose(g, 1.0f);
    rig.now += std::chrono::seconds(2);
    (void)rig.hands.update(moved, HandsRig::active(), {}, rig.now, 1.0f);
    r = rig.hands.aim(game, cam, {WeaponType::Pistol}, rig.now);
    REQUIRE(r.ray.has_value());
    check_close(r.ray->start, {5.3f, 0, 5});
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

// ---- The arm pose edit (21 §2.3, §3.5, §4.2) ---------------------------------------

using omvr::util::angle_between;

// The angle in radians between two unit quaternions' rotations.
float rotation_gap(const Quat& a, const Quat& b) { return angle_between(a, b); }

Quat about(Vec3 axis, float angle) { return axis_angle(axis, angle); }

// Synthetic finger bases in the hand bone's frame (21 §3.5): the fingers
// along `forward`'s sign of x, the palm facing -y, the knuckles from the
// little finger (z -0.03) to the index (z +0.03), the thumb below the palm
// on the index side. In the game's left-handed axes this is the hand whose
// fingers point along +x (right) or -x (left) with the palm down.
FingerBases fingers(float forward) {
    return {{0.09f * forward, 0, 0.03f},
            {0.09f * forward, 0, 0.01f},
            {0.09f * forward, 0, -0.01f},
            {0.09f * forward, 0, -0.03f},
            {0.03f * forward, -0.03f, 0.02f}};
}

TEST_CASE("grip frame: from the finger bones, the wrist about 9 cm from the grip (21 §3.5)") {
    // Expected from the spec's construction: a = unit(I - P) = +z; n on the
    // thumb's side = -y (the palm); the origin is the knuckles' mean
    // (0.09, 0, 0) moved 0.025 m along n; wrist to grip
    // sqrt(0.09^2 + 0.025^2) = 0.09341 m.
    const auto right = grip_frame(fingers(1.0f), kRightHand);
    REQUIRE(right.has_value());
    check_close(right->in_hand.translation, {0.09f, -0.025f, 0.0f});
    CHECK(right->wrist_distance_m == doctest::Approx(0.093408f).epsilon(1e-4));
    // The grip's local +Z (its -Z in the runtime's axes) runs along the
    // knuckles; +X is the palm normal the runtime gives the right hand (into
    // the palm: up out of the back of the hand); +Y then points from the
    // knuckles back to the wrist, as the runtime's grip +Y does for a hand
    // around a handle (OpenXR's right-hand rule; 21 §3.1).
    const Quat r = right->in_hand.rotation;
    check_close(omvr::util::rotate(r, {0, 0, 1}), {0, 0, 1});
    check_close(omvr::util::rotate(r, {1, 0, 0}), {0, 1, 0});
    check_close(omvr::util::rotate(r, {0, 1, 0}), {-1, 0, 0});

    // The left hand, mirrored (fingers along -x): its +X out of the palm
    // (down), +Y again from the knuckles to the wrist.
    const auto left = grip_frame(fingers(-1.0f), kLeftHand);
    REQUIRE(left.has_value());
    check_close(left->in_hand.translation, {-0.09f, -0.025f, 0.0f});
    const Quat l = left->in_hand.rotation;
    check_close(omvr::util::rotate(l, {0, 0, 1}), {0, 0, 1});
    check_close(omvr::util::rotate(l, {1, 0, 0}), {0, -1, 0});
    check_close(omvr::util::rotate(l, {0, 1, 0}), {1, 0, 0});

    // Degenerate bones (the knuckles on one point) span no hand.
    CHECK_FALSE(grip_frame({{0.09f, 0, 0}, {0.09f, 0, 0}, {0.09f, 0, 0}, {0.09f, 0, 0}, {}}, kRightHand));
}

TEST_CASE("grip to hand bone: the bone is placed so its grip frame is on the target (21 §3.5)") {
    const auto grip = grip_frame(fingers(1.0f), kRightHand);
    REQUIRE(grip.has_value());
    // A grip held level, its handle along the world's +z: the fingers point
    // down (-y), the back of the hand to +x, and the wrist sits above the
    // grip: target + (0.025, 0.09, 0).
    const RigidTransform hand = hand_for_grip({{1, 2, 3}, {}}, *grip);
    check_close(hand.translation, {1.025f, 2.09f, 3.0f});
    check_close(omvr::util::rotate(hand.rotation, {1, 0, 0}), {0, -1, 0});
    // The grip frame put back on the bone lands on the target.
    const Vec3 origin =
        omvr::util::add(hand.translation, omvr::util::rotate(hand.rotation, grip->in_hand.translation));
    check_close(origin, {1, 2, 3});
}

TEST_CASE("two-bone reach: a 0.3 m and 0.4 m arm reaches 0.5 m with a right angle at the elbow") {
    // 3-4-5: along = (0.09 - 0.16 + 0.25) / 1.0 = 0.18, height
    // sqrt(0.09 - 0.0324) = 0.24, the elbow bent towards the hint (-y).
    const Vec3 root{0, 0, 0}, elbow{0, 0, 0.3f}, wrist{0, 0, 0.7f};
    const auto s = solve_two_bone(root, elbow, wrist, {0.5f, 0, 0}, {0, -1, 0});
    CHECK(s.in_reach);
    check_close(s.elbow, {0.18f, -0.24f, 0});
    check_close(s.wrist, {0.5f, 0, 0});
    // The rotations carry the bones there: the upper arm about the root, the
    // forearm about the new elbow.
    check_close(omvr::util::rotate(s.upper, elbow), s.elbow);
    const Vec3 fore_after = omvr::util::rotate(s.upper, omvr::util::sub(wrist, elbow));
    check_close(omvr::util::add(s.elbow, omvr::util::rotate(s.fore, fore_after)), s.wrist);
}

TEST_CASE("two-bone reach: out of reach the arm is straight towards the target (21 §4.2 item 2)") {
    const auto s = solve_two_bone({0, 0, 0}, {0, 0, 0.3f}, {0, 0, 0.7f}, {2, 0, 0}, {0, -1, 0});
    CHECK_FALSE(s.in_reach);
    CHECK(s.wrist.x == doctest::Approx(0.7f).epsilon(1e-3));
    CHECK(s.elbow.x == doctest::Approx(0.3f).epsilon(1e-3));
    // Within a degree of straight (5 mm off the line over 0.3 m): the
    // slack keeps the solve off the singular straight line.
    CHECK(std::fabs(s.elbow.y) < 0.3f * std::sin(std::numbers::pi_v<float> / 180));
    CHECK(std::fabs(s.wrist.y) < kEps);
}

TEST_CASE("shoulder turn: none within 0.9 of the reach, at most 15 degrees beyond it (21 §4.2 item 3)") {
    const Vec3 shoulder{0, 0, 0}, root{0.1f, 0, 0};
    const Quat none = shoulder_turn(shoulder, root, 0.55f, {0.1f, 0.4f, 0});
    CHECK(none.w == doctest::Approx(1.0f));
    // Far above: the full 15 degrees about +z, towards the target.
    const Quat full = shoulder_turn(shoulder, root, 0.55f, {0.1f, 2.0f, 0});
    CHECK(rotation_gap(full, {}) == doctest::Approx(15.0f * std::numbers::pi_v<float> / 180).epsilon(1e-3));
    CHECK(omvr::util::rotate(full, root).y > 0.0f);
    // Halfway between 0.9 and 1.0 of the reach: half the turn.
    const float half_way = 0.55f * 0.95f;
    const Quat half      = shoulder_turn(shoulder, root, 0.55f, {0.1f, half_way, 0});
    CHECK(rotation_gap(half, {}) == doctest::Approx(7.5f * std::numbers::pi_v<float> / 180).epsilon(1e-2));
}

TEST_CASE("model to world: T + rotate(Q, S * model) and back (21 §2.3, r9)") {
    // Q a quarter turn about +y takes +x to -z (the quaternion's rotation).
    const ModelToWorld w{{1, 2, 3}, yaw(std::numbers::pi_v<float> / 2), {2, 2, 2}};
    check_close(to_world_point(w, {1, 0, 0}), {1, 2, 1});
    check_close(to_model_point(w, {1, 2, 1}), {1, 0, 0});
    check_close(to_model_direction(w, {0, 0, -1}), {1, 0, 0});
    const RigidTransform m = to_model(w, {{1, 2, 1}, yaw(std::numbers::pi_v<float> / 2)});
    check_close(m.translation, {1, 0, 0});
    CHECK(rotation_gap(m.rotation, {}) < 1e-3f);
    // A record whose scale is not set is taken as unscaled.
    const ModelToWorld unset{{1, 2, 3}, {}, {0, 0, 0}};
    check_close(to_world_point(unset, {1, 0, 0}), {2, 2, 3});
    check_close(to_model_point(unset, {2, 2, 3}), {1, 0, 0});
}

TEST_CASE("twist about an axis; drift follows the body (21 §4.2 items 3, 10)") {
    const Vec3 x{1, 0, 0};
    CHECK(twist_angle(about(x, 0.9f), x) == doctest::Approx(0.9f));
    // A swing about another axis adds no twist.
    CHECK(twist_angle(about({0, 1, 0}, 0.7f), x) == doctest::Approx(0.0f));
    CHECK(twist_angle(omvr::util::multiply(about({0, 0, 1}, 0.3f), about(x, -0.5f)), x) ==
          doctest::Approx(-0.5f).epsilon(0.05));
    // 21 §4.2 item 10: a target placed while the body was at (1, 0, 0) and
    // drawn with it at (1.05, 0, 0.02) moves by the body's 5 cm and 2 cm.
    check_close(follow_body({2, 1, 3}, {1, 0, 0}, {1.05f, 0, 0.02f}), {2.05f, 1, 3.02f});
}

// A small two-armed skeleton with the bone names of 21 §2.3: the root, the
// head, then each side's shoulder, arm, forearm, its two roll bones, the
// hand and the five finger bases. Every bone unrotated; the arms reach out
// sideways (the left along -x) at shoulder height.
struct TestSkeleton {
    static constexpr std::size_t kSide  = 12; // bones per side
    static constexpr std::size_t kCount = 2 + 2 * kSide;
    std::array<std::string_view, kCount> names{};
    std::array<std::int16_t, kCount> parents{};
    std::array<BoneTransform, kCount> local{};
    std::array<BoneTransform, kCount> model{};
    std::array<std::uint32_t, kCount> flags{};

    TestSkeleton() {
        static constexpr std::array<std::array<std::string_view, kSide>, 2> kNames{{
            {"LeftShoulder", "LeftArm", "LeftForeArm", "LeftForeArmRoll", "LeftForeArmRoll1", "LeftHand",
             "LeftHandIndex1", "LeftHandMiddle1", "LeftHandRing1", "LeftHandPinky1", "LeftHandThumb1",
             "LeftWeaponBone"},
            {"RightShoulder", "RightArm", "RightForeArm", "RightForeArmRoll", "RightForeArmRoll1",
             "RightHand", "RightHandIndex1", "RightHandMiddle1", "RightHandRing1", "RightHandPinky1",
             "RightHandThumb1", "RightWeaponBone"},
        }};
        names[0]   = "";
        names[1]   = "Head";
        parents[0] = -1;
        parents[1] = 0;
        set(0, {0, 0, 0});
        set(1, {0, 1.5f, 0});
        for (std::size_t h = 0; h < 2; ++h) {
            const float s       = h == 0 ? -1.0f : 1.0f;
            const auto b        = [&](std::size_t k) { return static_cast<std::int16_t>(2 + h * kSide + k); };
            const FingerBases f = fingers(s);
            const std::array<std::pair<std::int16_t, Vec3>, kSide> bones{{
                {1, {0.1f * s, -0.1f, 0}}, // shoulder: from the head
                {b(0), {0.1f * s, 0, 0}},  // arm: 0.3 m upper arm
                {b(1), {0.3f * s, 0, 0}},  // forearm: 0.25 m
                {b(2), {0.1f * s, 0, 0}},  // roll
                {b(2), {0.2f * s, 0, 0}},  // roll1
                {b(2), {0.25f * s, 0, 0}}, // hand
                {b(5), f.index},
                {b(5), f.middle},
                {b(5), f.ring},
                {b(5), f.pinky},
                {b(5), f.thumb},
                {b(5), {0.05f * s, -0.03f, 0}}, // weapon bone
            }};
            for (std::size_t k = 0; k < kSide; ++k) {
                names[b(k)]   = kNames[h][k];
                parents[b(k)] = bones[k].first;
                set(static_cast<std::size_t>(b(k)), bones[k].second);
            }
        }
        for (std::size_t i = 0; i < kCount; ++i)
            model[i] =
                parents[i] < 0 ? local[i] : compose(model[static_cast<std::size_t>(parents[i])], local[i]);
    }

    void set(std::size_t i, Vec3 t) {
        local[i]             = {};
        local[i].translation = {t.x, t.y, t.z, 0};
        local[i].rotation    = {0, 0, 0, 1};
        local[i].scale       = {1, 1, 1, 1};
    }

    SkeletonBones bones(bool from_table = false) const { return {names, parents, from_table}; }
    PoseArrays arrays() { return {local, model, flags, parents}; }
    Vec3 position(std::int16_t bone) const {
        return rigid_of(model[static_cast<std::size_t>(bone)]).translation;
    }

    // Every model transform is its parent's composed with its local one.
    void check_consistent() const {
        for (std::size_t i = 1; i < kCount; ++i) {
            const BoneTransform m = compose(model[static_cast<std::size_t>(parents[i])], local[i]);
            check_close(rigid_of(m).translation, rigid_of(model[i]).translation);
            CHECK(rotation_gap(rigid_of(m).rotation, rigid_of(model[i]).rotation) < 1e-3f);
            CHECK((flags[i] & (pose_flag::kLocalStale | pose_flag::kModelStale)) == 0);
        }
    }
};

TEST_CASE("arm rig: the bones by name, in the first-person table at hands 9 and 32 (21 §2.3)") {
    const omvr::core::hands::SkeletonBones table{omvr::game::hands::kFirstPersonBoneNames,
                                                 omvr::game::hands::kFirstPersonBoneParents, true};
    const RigResult r = find_arm_rig(table);
    REQUIRE(r.rig.has_value());
    const ArmSideBones& l  = r.rig->sides[kLeftHand];
    const ArmSideBones& rr = r.rig->sides[kRightHand];
    CHECK(l.shoulder == 3);
    CHECK(l.arm == 4);
    CHECK(l.forearm == 6);
    CHECK(l.roll == 7);
    CHECK(l.roll1 == 8);
    CHECK(l.hand == 9);
    CHECK(l.middle == 13);
    CHECK(rr.shoulder == 26);
    CHECK(rr.arm == 27);
    CHECK(rr.forearm == 29);
    CHECK(rr.hand == 32);
    CHECK(rr.thumb == 45);
    CHECK(r.rig->head == 1);
    CHECK(r.rig->first_person);

    // A skeleton without a finger base by name is not used for the arms.
    TestSkeleton s;
    s.names[2 + 6]          = "LeftHandIndex";
    const RigResult missing = find_arm_rig(s.bones());
    CHECK_FALSE(missing.rig.has_value());
    CHECK(missing.fault == RigFault::MissingBone);
    CHECK(missing.bone == "HandIndex1");
}

TEST_CASE("pose edit: the hand reaches its goal, the arm keeps its lengths, the pose agrees (21 §4.2)") {
    TestSkeleton s;
    const RigResult r = find_arm_rig(s.bones());
    REQUIRE(r.rig.has_value());
    const ArmSideBones& right = r.rig->sides[kRightHand];
    PoseArrays p              = s.arrays();
    REQUIRE(prepare_arms(p, *r.rig) == PoseFault::None);

    // In reach of the 0.55 m arm from (0.2, 1.4, 0), twisted about the
    // forearm's axis.
    const RigidTransform goal{{0.5f, 1.2f, 0.3f}, about({1, 0, 0}, 1.2f)};
    const Vec3 finger_before = s.position(right.index);
    const ArmOutcome o       = pose_arm(p, right, {goal, 1.0f, {0, -1, 0}});
    refresh_arms(p, *r.rig);
    CHECK(o.edited);
    CHECK(o.in_reach);
    // 21 §4.2 item 1: within 5 mm and 2 degrees (here exact up to float).
    CHECK(omvr::util::distance(s.position(right.hand), goal.translation) < kHandPrecisionM / 10);
    CHECK(rotation_gap(rigid_of(s.model[static_cast<std::size_t>(right.hand)]).rotation, goal.rotation) <
          1e-3f);
    // The shoulder and the upper arm's root stay; the bones keep their
    // lengths; the elbow bends down (the hint).
    check_close(s.position(right.shoulder), {0.1f, 1.4f, 0});
    check_close(s.position(right.arm), {0.2f, 1.4f, 0});
    CHECK(omvr::util::distance(s.position(right.forearm), s.position(right.arm)) == doctest::Approx(0.3f));
    CHECK(omvr::util::distance(s.position(right.hand), s.position(right.forearm)) == doctest::Approx(0.25f));
    CHECK(s.position(right.forearm).y < 1.4f);
    // 21 §4.2 item 5: the fingers keep their pose in the hand; they moved
    // with it.
    CHECK(omvr::util::distance(s.position(right.index), finger_before) > 0.1f);
    CHECK(omvr::util::distance(s.position(right.index), s.position(right.hand)) ==
          doctest::Approx(omvr::util::length(fingers(1.0f).index)));
    // 21 §4.2 item 3 (r11): the rolls take a third and two thirds of the
    // hand's twist about the forearm; the hand all of it.
    const Vec3 axis   = {1, 0, 0}; // the hand's offset in the forearm's frame
    const float twist = twist_angle(rigid_of(s.local[static_cast<std::size_t>(right.hand)]).rotation, axis);
    CHECK(std::fabs(twist) > 0.5f);
    CHECK(twist_angle(rigid_of(s.local[static_cast<std::size_t>(right.roll)]).rotation, axis) ==
          doctest::Approx(twist / 3).epsilon(1e-3));
    CHECK(twist_angle(rigid_of(s.local[static_cast<std::size_t>(right.roll1)]).rotation, axis) ==
          doctest::Approx(twist * 2 / 3).epsilon(1e-3));
    // 21 §4.2 item 9: local and model transforms agree; the other arm is
    // untouched.
    s.check_consistent();
    check_close(s.position(r.rig->sides[kLeftHand].hand), {-0.75f, 1.4f, 0});
}

TEST_CASE("pose edit: out of reach straight at the target; a weight blends from the game's hand") {
    TestSkeleton s;
    const RigResult r = find_arm_rig(s.bones());
    REQUIRE(r.rig.has_value());
    const ArmSideBones& right = r.rig->sides[kRightHand];
    PoseArrays p              = s.arrays();
    REQUIRE(prepare_arms(p, *r.rig) == PoseFault::None);
    const ArmOutcome far = pose_arm(p, right, {{{2.0f, 1.4f, 0}, about({0, 0, 1}, 0.4f)}, 1.0f, {0, -1, 0}});
    refresh_arms(p, *r.rig);
    CHECK_FALSE(far.in_reach);
    CHECK(s.position(right.hand).x == doctest::Approx(0.75f).epsilon(1e-3));
    CHECK(rotation_gap(rigid_of(s.model[static_cast<std::size_t>(right.hand)]).rotation,
                       about({0, 0, 1}, 0.4f)) < 1e-3f);

    // Half the weight: the goal halfway from the game's hand.
    TestSkeleton t;
    PoseArrays q = t.arrays();
    REQUIRE(prepare_arms(q, *r.rig) == PoseFault::None);
    const ArmOutcome half = pose_arm(q, right, {{{0.55f, 1.2f, 0}, {}}, 0.5f, {0, -1, 0}});
    refresh_arms(q, *r.rig);
    check_close(half.goal.translation, {0.65f, 1.3f, 0});
    CHECK(omvr::util::distance(t.position(right.hand), {0.65f, 1.3f, 0}) < 1e-3f);
    // 0 is no edit.
    CHECK_FALSE(pose_arm(q, right, {{{0.3f, 1.0f, 0}, {}}, 0.0f, {0, -1, 0}}).edited);
}

TEST_CASE("pose edit: an out-of-date local transform is made current; an out-of-date model is refused") {
    TestSkeleton s;
    const RigResult r = find_arm_rig(s.bones());
    REQUIRE(r.rig.has_value());
    const auto hand = static_cast<std::size_t>(r.rig->sides[kRightHand].hand);
    // The game left the hand's local transform out of date (its model one
    // is the truth).
    s.local[hand].translation = {9, 9, 9, 0};
    s.flags[hand]             = pose_flag::kLocalStale;
    PoseArrays p              = s.arrays();
    REQUIRE(prepare_arms(p, *r.rig) == PoseFault::None);
    CHECK(s.local[hand].translation[0] == doctest::Approx(0.25f));
    CHECK(s.flags[hand] == 0);

    s.flags[hand] = pose_flag::kModelStale;
    CHECK(prepare_arms(p, *r.rig) == PoseFault::Stale);
    PoseArrays short_pose{std::span{s.local}.first(5), s.model, s.flags, s.parents};
    CHECK(prepare_arms(short_pose, *r.rig) == PoseFault::Size);
}

TEST_CASE("ArmPoser: the job edits once per frame from the published targets; the report is drawn") {
    omvr::tests::RecordingLog log;
    ArmPoser arms{log};
    TestSkeleton s;
    const PlayerSkeleton player{0x1000, 0x2000, 0x3000, "TESTFP"};
    TimePoint now{std::chrono::seconds(50)};
    REQUIRE(arms.wants_rig(player, now));
    const SkeletonBones bones = s.bones(true);
    arms.set_rig(player, &bones, {}, now);
    CHECK_FALSE(arms.wants_rig(player, now));
    CHECK(log.contains(
        "hands: the arms of skeleton TESTFP follow the controllers (bones found by name, names "
        "from the first-person table): left shoulder 2 arm 3 forearm 4 rolls 5 6 hand 7, right "
        "shoulder 14 arm 15 forearm 16 rolls 17 18 hand 19; the game's limb IK is not run for it "
        "while tracked hands are active"));

    // Nothing published: no frame, the limb IK runs.
    CHECK(arms.pending() == nullptr);
    CHECK_FALSE(arms.limb_ik_off(player.skeleton));

    // The right grip held level ahead of the right shoulder; the camera at
    // the head looking along +z; the body's model-to-world the identity
    // except a step of 2 cm along +x since the targets were placed
    // (21 §4.2 item 10).
    const CameraPose camera{{}, {0, 1.5f, 0}};
    ModelToWorld w;
    ArmTargets targets;
    targets[kRightHand] = ArmTarget{{{0.45f, 1.1f, 0.25f}, {}}, 1.0f};
    arms.publish(player, camera, w, targets, now);
    CHECK(arms.limb_ik_off(player.skeleton));
    const ArmFrame* f = arms.pending();
    REQUIRE(f != nullptr);
    CHECK(arms.claim(*f));
    CHECK_FALSE(arms.claim(*f)); // one job per frame
    w.translation   = {0.02f, 0, 0};
    PoseArrays pose = s.arrays();
    arms.apply(*f, w, pose);
    CHECK(arms.pending() == nullptr);
    CHECK(arms.right_edited());

    // The grip frame lands where the target moved with the body.
    const auto grip    = grip_frame(fingers(1.0f), kRightHand);
    const auto hand    = static_cast<std::size_t>(find_arm_rig(bones).rig->sides[kRightHand].hand);
    const auto drawn   = rigid_of(s.model[hand]);
    const Vec3 at_grip = omvr::util::add(omvr::util::add(drawn.translation, w.translation),
                                         omvr::util::rotate(drawn.rotation, grip->in_hand.translation));
    check_close(at_grip, {0.47f, 1.1f, 0.25f});

    REQUIRE(arms.head_check_due().has_value());
    CHECK(*arms.head_check_due() == 1);
    arms.after_pass(Vec3{0, 1.55f, 0.05f}, camera, true, now);
    CHECK(log.contains(
        "hands: grip frame of TESTFP, right hand: wrist to grip 0.093 m (expected 0.07 to 0.12 m)"));
    CHECK(log.contains(
        "hands: Head bone 1 of TESTFP at (0.000 1.550 0.050) in the world (BoneWorldTransform, "
        "final pose), active camera at (0.000 1.500 0.000): 0.071 m apart (expected within 0.25 m)"));
    CHECK(log.contains("hands: the player's arms are drawn from the edited pose"));
    CHECK(log.contains("hands: the held item follows the moved right hand"));
    CHECK(arms.weapon_in_moved_hand(now));
    CHECK_FALSE(arms.weapon_in_moved_hand(now + std::chrono::seconds(1)));
    // A later pass whose right hand did not carry the item: not in the
    // moved hand from that frame on, not kStaleAfter later.
    arms.after_pass(std::nullopt, camera, false, now + std::chrono::milliseconds(11));
    CHECK_FALSE(arms.weapon_in_moved_hand(now + std::chrono::milliseconds(11)));
    arms.after_pass(std::nullopt, camera, true, now + std::chrono::milliseconds(22));
    CHECK(arms.weapon_in_moved_hand(now + std::chrono::milliseconds(22)));
    // The left arm was not edited: no drawn left hand for the tracker.
    CHECK_FALSE(arms.drawn_left(now).has_value());

    // Hands off (nothing published): the limb IK runs again, no frame.
    arms.publish_none();
    CHECK_FALSE(arms.limb_ik_off(player.skeleton));
    CHECK_FALSE(arms.weapon_in_moved_hand(now + std::chrono::milliseconds(22)));
    CHECK(arms.pending() == nullptr);
}

TEST_CASE("ArmPoser: the drawn left hand points from the hand bone to the middle finger (21 §6.2)") {
    omvr::tests::RecordingLog log;
    ArmPoser arms{log};
    TestSkeleton s;
    const PlayerSkeleton player{0x1000, 0x2000, 0x3000, "TESTFP"};
    TimePoint now{std::chrono::seconds(50)};
    const SkeletonBones bones = s.bones();
    arms.set_rig(player, &bones, {}, now);
    // A rig with its own names: the game's limb IK is left alone.
    CHECK(log.contains("the game's limb IK runs for it as in v1"));
    ArmTargets targets;
    // The left grip held level ahead: the handle along +z, so the fingers
    // point down and the knuckles ahead of the wrist point down too.
    targets[kLeftHand] = ArmTarget{{{-0.4f, 1.1f, 0.3f}, {}}, 1.0f};
    arms.publish(player, {{}, {0, 1.5f, 0}}, {}, targets, now);
    CHECK_FALSE(arms.limb_ik_off(player.skeleton));
    const ArmFrame* f = arms.pending();
    REQUIRE(f != nullptr);
    REQUIRE(arms.claim(*f));
    PoseArrays pose = s.arrays();
    arms.apply(*f, {}, pose);
    const auto dir = arms.drawn_left(now);
    REQUIRE(dir.has_value());
    CHECK(dir->y < -0.08f);
    CHECK(std::fabs(dir->x) < 1e-3f);
    CHECK_FALSE(arms.drawn_left(now + std::chrono::seconds(1)).has_value());
}

TEST_CASE("ArmPoser: a skeleton without the arms' bones is logged once and not edited (21 §4.6)") {
    omvr::tests::RecordingLog log;
    ArmPoser arms{log};
    TestSkeleton s;
    s.names[2 + 5] = "LeftPaw";
    const PlayerSkeleton player{0x1000, 0x2000, 0x3000, "DLCFP"};
    TimePoint now{std::chrono::seconds(50)};
    const SkeletonBones bones = s.bones();
    arms.set_rig(player, &bones, {}, now);
    arms.set_rig(player, &bones, {}, now);
    CHECK(log.contains(
        "hands: WARNING the arms of skeleton DLCFP keep the game's animation: a bone is missing by "
        "name: Hand"));
    CHECK(log.lines.size() == 1);
    ArmTargets targets;
    targets[kLeftHand] = ArmTarget{{{-0.4f, 1.1f, 0.3f}, {}}, 1.0f};
    arms.publish(player, {}, {}, targets, now);
    CHECK(arms.pending() == nullptr);
    // A skeleton that cannot be read yet is read again a second later.
    const PlayerSkeleton other{0x1000, 0x2000, 0x4000, "FEMALEFP"};
    arms.set_rig(other, nullptr, {SkeletonFault::NoBones, 0, 0}, now);
    CHECK(
        log.contains("hands: WARNING the arms of skeleton FEMALEFP keep the game's animation until it can be "
                     "read: the skeleton has no bones; it is read again every 1 s"));
    CHECK_FALSE(arms.wants_rig(other, now));
    CHECK(arms.wants_rig(other, now + std::chrono::seconds(1)));
}

TEST_CASE("Hands: arm targets are the tracked grips in the world, weighted; none with the key off") {
    HandsRig rig;
    const CameraPose cam{yaw(std::numbers::pi_v<float> / 2), {5, 0, 5}};
    rig.run(std::chrono::milliseconds(11), 5, {});
    CHECK_FALSE(rig.hands.arm_targets(cam, rig.now)[kRightHand].has_value());
    rig.settings.enabled = true;
    rig.run(std::chrono::milliseconds(11), 30, HandsRig::active());
    const ArmTargets t = rig.hands.arm_targets(cam, rig.now);
    REQUIRE(t[kRightHand].has_value());
    REQUIRE(t[kLeftHand].has_value());
    // 0.5 m ahead of a camera at (5, 0, 5) looking along +x.
    check_close(t[kRightHand]->grip.position, {5.5f, 0, 5});
    CHECK(t[kRightHand]->weight == doctest::Approx(1.0f));
    // Stale (no update for a second): none.
    CHECK_FALSE(rig.hands.arm_targets(cam, rig.now + std::chrono::seconds(1))[kLeftHand].has_value());
}

} // namespace
