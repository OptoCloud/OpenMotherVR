// OpenMotherVR-Provenance: clean
// core/input: the body gestures of spec 22 §8.1 (common rules), §8.2 (the
// two-handed recenter), §8.4 (holsters) and §9 (lean and hold breath while
// hiding), and the control mapper applying them. Expected values are the
// spec's distances and times.

#include "core/config/settings.h"
#include "core/input/control_mapper.h"
#include "core/input/control_scheme.h"
#include "core/input/gesture_geometry.h"
#include "core/input/gestures.h"
#include "core/input/glyphs.h"
#include "core/input/virtual_pad.h"
#include "fakes/fakes.h"
#include "game/constants.h"

#include <doctest/doctest.h>

#include <array>
#include <chrono>
#include <cmath>
#include <numbers>

namespace {

using namespace omvr::core::input;
namespace game = omvr::game;
namespace pa   = omvr::game::player_action;
using namespace std::chrono_literals;
using omvr::tests::FakeClock;
using omvr::tests::RecordingLog;

constexpr float kQuarterTurn = std::numbers::pi_v<float> / 2.0f;
const GestureTime kT0{std::chrono::seconds(100)};

class FakeContext final : public ports::GestureContext {
public:
    ports::GameContext game_context() const noexcept override { return state; }
    ports::GameContext state;
};

class FreeStick final : public ports::StickContext {
public:
    bool stick_free() const noexcept override { return false; }
};

// A standing player facing -Z with the head at 1.6 m, focused.
BodyPoses standing() {
    BodyPoses p;
    p.head       = {0.0f, 1.6f, 0.0f};
    p.head_yaw   = 0.0f;
    p.head_valid = true;
    p.focused    = true;
    return p;
}

// Hand `h` at `offset` from the head, in the head frame (yaw 0: tracking axes).
void place(BodyPoses& p, std::size_t h, Vec3 offset) {
    p.hands[h]      = {p.head.x + offset.x, p.head.y + offset.y, p.head.z + offset.z};
    p.hand_valid[h] = true;
}

} // namespace

// ---- 22 §8.1 -------------------------------------------------------------------

TEST_CASE("gestures: the head frame turns with the head's yaw only") {
    // Facing -X (yaw +90°): in front is -X, the right hand side is -Z.
    const HeadFrame f{{1.0f, 1.6f, 2.0f}, kQuarterTurn};
    const Vec3 front = f.local({0.5f, 1.6f, 2.0f});
    CHECK(front.x == doctest::Approx(0.0f).epsilon(1e-5));
    CHECK(front.z == doctest::Approx(-0.5f));
    const Vec3 right = f.local({1.0f, 1.3f, 1.5f});
    CHECK(right.x == doctest::Approx(0.5f));
    CHECK(right.y == doctest::Approx(-0.3f));
    const Vec3 back  = f.world({0.2f, -0.1f, 0.3f});
    const Vec3 again = f.local(back);
    CHECK(again.x == doctest::Approx(0.2f));
    CHECK(again.y == doctest::Approx(-0.1f));
    CHECK(again.z == doctest::Approx(0.3f));
}

TEST_CASE("gestures: the head yaw is defined at any pitch") {
    CHECK(head_yaw({0.0f, 0.0f, -1.0f}, {0.0f, 1.0f, 0.0f}) == doctest::Approx(0.0f));
    CHECK(head_yaw({-1.0f, 0.0f, 0.0f}, {0.0f, 1.0f, 0.0f}) == doctest::Approx(kQuarterTurn));
    // Facing -X and looking straight up: the up axis leans back (+X).
    CHECK(head_yaw({0.0f, 1.0f, 0.0f}, {1.0f, 0.0f, 0.0f}) == doctest::Approx(kQuarterTurn));
    // Facing -X and looking straight down: the up axis leans forward (-X).
    CHECK(head_yaw({0.0f, -1.0f, 0.0f}, {-1.0f, 0.0f, 0.0f}) == doctest::Approx(kQuarterTurn));
}

TEST_CASE("gestures: a hand without a valid pose takes no part") {
    BodyPoses p = standing();
    place(p, kLeftHand, {0.1f, 0.0f, 0.0f});
    CHECK(hand_near_head(p, kLeftHand, 0.2f));
    CHECK_FALSE(hand_near_head(p, kLeftHand, 0.05f));
    CHECK_FALSE(hand_near_head(p, kRightHand, 0.2f)); // never placed
    p.head_valid = false;
    CHECK_FALSE(hand_near_head(p, kLeftHand, 0.2f));
    CHECK_FALSE(hand_in_head_frame(p, kLeftHand).has_value());
}

TEST_CASE("gestures: nothing fires without focus nor in the 0.5 s after it returns") {
    FocusGate gate;
    CHECK_FALSE(gate.open(false, kT0));
    CHECK_FALSE(gate.open(true, kT0 + 1s));
    CHECK_FALSE(gate.open(true, kT0 + 1s + 499ms));
    CHECK(gate.open(true, kT0 + 1s + 500ms));
    CHECK_FALSE(gate.open(false, kT0 + 2s));
    CHECK_FALSE(gate.open(true, kT0 + 3s + 400ms));
    CHECK(gate.open(true, kT0 + 3s + 900ms));
}

// ---- 22 §8.2 -------------------------------------------------------------------

TEST_CASE("recenter gesture: 0.75 s of both grips at the head fires once") {
    RecenterGesture g;
    const RecenterGesture::Input held{.grips = true, .near = true, .fire = false, .open = true};
    using R = RecenterGesture::Result;
    CHECK(g.update(true, held, kT0) == R::None);
    CHECK(g.update(true, held, kT0 + 749ms) == R::None);
    CHECK(g.update(true, held, kT0 + 750ms) == R::Fired);
    CHECK(g.update(true, held, kT0 + 3s) == R::None); // once per performance
    CHECK(g.update(true, {.grips = false, .near = true, .open = true}, kT0 + 4s) == R::None);
    CHECK(g.update(true, held, kT0 + 5s) == R::None);
    CHECK(g.update(true, held, kT0 + 5s + 750ms) == R::Fired);
}

TEST_CASE("recenter gesture: the hold restarts whenever a condition breaks") {
    RecenterGesture g;
    using R = RecenterGesture::Result;
    const RecenterGesture::Input held{.grips = true, .near = true, .fire = false, .open = true};
    CHECK(g.update(true, held, kT0) == R::None);
    CHECK(g.update(true, {.grips = true, .near = false, .open = true}, kT0 + 500ms) == R::None);
    CHECK(g.update(true, held, kT0 + 600ms) == R::None);
    CHECK(g.update(true, held, kT0 + 1s) == R::None); // 0.4 s since the restart
    CHECK(g.update(true, held, kT0 + 1350ms) == R::Fired);

    // Fire pressed: never, and the hold starts over once it is released.
    RecenterGesture f;
    CHECK(f.update(true, held, kT0) == R::None);
    auto firing = held;
    firing.fire = true;
    CHECK(f.update(true, firing, kT0 + 500ms) == R::SuppressedFire);
    CHECK(f.update(true, firing, kT0 + 2s) == R::None); // reported once
    CHECK(f.update(true, held, kT0 + 2100ms) == R::None);
    CHECK(f.update(true, held, kT0 + 2849ms) == R::None);
    CHECK(f.update(true, held, kT0 + 2850ms) == R::Fired);

    // Off: never.
    RecenterGesture off;
    CHECK(off.update(false, held, kT0) == R::None);
    CHECK(off.update(false, held, kT0 + 2s) == R::None);
}

// ---- 22 §8.4 -------------------------------------------------------------------

TEST_CASE("holster zones") {
    CHECK(holster_zone(kRightHand, {0.15f, -0.1f, 0.15f}) == Holster::RightShoulder);
    CHECK(holster_zone(kLeftHand, {-0.15f, -0.1f, 0.15f}) == Holster::LeftShoulder);
    CHECK(holster_zone(kRightHand, {0.15f, -0.3f, 0.15f}) == Holster::None); // below the shoulder
    CHECK(holster_zone(kRightHand, {0.15f, -0.1f, 0.03f}) == Holster::None); // not behind the head
    CHECK(holster_zone(kLeftHand, {-0.25f, -0.65f, 0.0f}) == Holster::LeftHip);
    CHECK(holster_zone(kLeftHand, {-0.25f, -0.65f, -0.05f}) == Holster::LeftHip);
    CHECK(holster_zone(kRightHand, {-0.25f, -0.65f, 0.0f}) == Holster::None);  // the left hand's
    CHECK(holster_zone(kLeftHand, {-0.15f, -0.65f, 0.0f}) == Holster::None);   // not left enough
    CHECK(holster_zone(kLeftHand, {-0.25f, -0.58f, 0.0f}) == Holster::None);   // not low enough
    CHECK(holster_zone(kLeftHand, {-0.25f, -0.65f, -0.15f}) == Holster::None); // in front of the body
    // The chest: a hand against it, also with the head bent down to it.
    CHECK(holster_zone(kLeftHand, {0.0f, -0.30f, -0.08f}) == Holster::Chest);
    CHECK(holster_zone(kRightHand, {0.1f, -0.40f, -0.15f}) == Holster::Chest);
    CHECK(holster_zone(kRightHand, {0.0f, -0.24f, 0.02f}) == Holster::Chest);
    CHECK(holster_zone(kRightHand, {0.0f, -0.35f, -0.25f}) == Holster::None); // in front of the chest
    CHECK(holster_zone(kRightHand, {0.0f, -0.35f, 0.08f}) == Holster::None);  // behind it
    CHECK(holster_zone(kRightHand, {0.2f, -0.35f, -0.1f}) == Holster::None);  // off centre
    CHECK(holster_zone(kRightHand, {0.0f, -0.15f, -0.1f}) == Holster::None);  // above the chest
    CHECK(holster_zone(kRightHand, {0.0f, -0.50f, -0.1f}) == Holster::None);  // below it
}

// The hands' ordinary hold in play, as a tester's log gave it (head-local,
// Index controllers through OpenXR, `openxr: hands` lines): a grip there
// must aim or raise the tracker, never select from a holster. The log's
// frame turns with the head's pitch too; the head frame of 22 §8.1 does
// not, so each position is also checked as it is with the head bent down.
TEST_CASE("holster zones: the ordinary hold in front of the body is in none") {
    struct Hands {
        Vec3 left, right;
    };
    constexpr std::array kLogged = {
        Hands{{-0.05f, -0.51f, -0.34f}, {0.14f, -0.51f, -0.32f}}, // the reported hold
        Hands{{-0.06f, -0.48f, -0.35f}, {0.07f, -0.50f, -0.34f}},
        Hands{{-0.07f, -0.51f, -0.35f}, {0.06f, -0.51f, -0.34f}},
        Hands{{-0.00f, -0.35f, -0.37f}, {0.23f, -0.30f, -0.33f}},
        Hands{{-0.09f, -0.19f, -0.43f}, {0.08f, -0.20f, -0.43f}},
        Hands{{0.09f, -0.51f, -0.32f}, {0.22f, -0.51f, -0.26f}},
        Hands{{-0.02f, -0.52f, -0.31f}, {0.14f, -0.52f, -0.28f}},
        Hands{{-0.01f, -0.49f, -0.35f}, {0.17f, -0.48f, -0.32f}},
        Hands{{-0.04f, -0.51f, -0.35f}, {0.14f, -0.50f, -0.32f}},
        Hands{{-0.00f, -0.57f, -0.27f}, {0.19f, -0.55f, -0.23f}},
        Hands{{-0.09f, -0.47f, -0.37f}, {0.11f, -0.46f, -0.37f}},
        Hands{{-0.08f, -0.54f, -0.32f}, {0.12f, -0.52f, -0.31f}},
        Hands{{-0.05f, -0.53f, -0.33f}, {0.15f, -0.52f, -0.31f}},
        Hands{{-0.04f, -0.49f, -0.37f}, {0.16f, -0.48f, -0.35f}},
        Hands{{-0.11f, -0.52f, -0.33f}, {0.09f, -0.51f, -0.34f}},
        Hands{{-0.17f, -0.37f, -0.23f}, {0.00f, -0.47f, -0.26f}},
        Hands{{-0.13f, -0.26f, -0.39f}, {0.14f, -0.23f, -0.37f}},
        Hands{{-0.13f, -0.45f, -0.29f}, {0.01f, -0.49f, -0.33f}},
        Hands{{-0.06f, -0.52f, -0.31f}, {0.08f, -0.52f, -0.31f}},
        Hands{{-0.08f, -0.52f, -0.31f}, {0.07f, -0.53f, -0.32f}},
        // The head turned to the right of the body, and hands in the lap.
        Hands{{-0.25f, -0.58f, -0.12f}, {-0.10f, -0.60f, -0.18f}},
        Hands{{-0.34f, -0.56f, -0.05f}, {-0.20f, -0.58f, -0.17f}},
        Hands{{-0.25f, -0.56f, -0.19f}, {-0.09f, -0.56f, -0.27f}},
        Hands{{0.05f, -0.63f, -0.13f}, {0.23f, -0.60f, -0.09f}},
        Hands{{-0.15f, -0.63f, -0.09f}, {-0.02f, -0.63f, -0.10f}},
    };
    // A head-local offset (y, z) in the yaw-only head frame when the head is
    // pitched down by `degrees`.
    const auto level = [](Vec3 p, float degrees) {
        const float t = degrees * std::numbers::pi_v<float> / 180.0f;
        return Vec3{p.x, p.y * std::cos(t) + p.z * std::sin(t), -p.y * std::sin(t) + p.z * std::cos(t)};
    };
    for (const Hands& h : kLogged) {
        for (const float pitch : {0.0f, 10.0f, 20.0f, 30.0f}) {
            CAPTURE(pitch);
            CAPTURE(h.left.x);
            CAPTURE(h.left.y);
            CAPTURE(h.left.z);
            CHECK(holster_zone(kLeftHand, level(h.left, pitch)) == Holster::None);
            CHECK(holster_zone(kRightHand, level(h.right, pitch)) == Holster::None);
        }
    }
}

TEST_CASE("holsters: a grip press that begins in a zone holds the action until released") {
    Holsters h;
    Holsters::Input in{.open = true};
    in.hands[kRightHand] = {.grip = true, .position = Vec3{0.15f, -0.1f, 0.15f}};
    auto r               = h.update(true, in);
    REQUIRE(r.event.has_value());
    CHECK(r.event->fired);
    CHECK(r.held[kRightHand] == Holster::RightShoulder);
    CHECK(r.consumed[kRightHand]);
    // Held while the hand moves away.
    in.hands[kRightHand].position = Vec3{0.2f, -0.4f, -0.3f};
    r                             = h.update(true, in);
    CHECK(r.held[kRightHand] == Holster::RightShoulder);
    CHECK_FALSE(r.event.has_value());
    in.hands[kRightHand].grip = false;
    r                         = h.update(true, in);
    CHECK(r.held[kRightHand] == Holster::None);
    CHECK_FALSE(r.consumed[kRightHand]);

    // A press that begins outside every zone is v1's, even moved into one.
    in.hands[kRightHand] = {.grip = true, .position = Vec3{0.2f, -0.4f, -0.4f}};
    r                    = h.update(true, in);
    CHECK_FALSE(r.consumed[kRightHand]);
    in.hands[kRightHand].position = Vec3{0.15f, -0.1f, 0.15f};
    r                             = h.update(true, in);
    CHECK_FALSE(r.consumed[kRightHand]);
    CHECK(r.held[kRightHand] == Holster::None);
}

TEST_CASE("holsters: off during interactions and menus") {
    Holsters h;
    Holsters::Input in{.blocked = true, .open = true};
    in.hands[kLeftHand] = {.grip = true, .position = Vec3{-0.25f, -0.65f, 0.0f}};
    auto r              = h.update(true, in);
    REQUIRE(r.event.has_value());
    CHECK_FALSE(r.event->fired);
    CHECK_FALSE(r.consumed[kLeftHand]);

    // An interaction beginning during a hold lets the action go; the grip
    // stays the holster's until released.
    Holsters k;
    Holsters::Input play{.open = true};
    play.hands[kLeftHand] = {.grip = true, .position = Vec3{-0.25f, -0.65f, 0.0f}};
    CHECK(k.update(true, play).held[kLeftHand] == Holster::LeftHip);
    play.blocked = true;
    r            = k.update(true, play);
    CHECK(r.held[kLeftHand] == Holster::None);
    CHECK(r.consumed[kLeftHand]);
}

// ---- 22 §9.2 -------------------------------------------------------------------

TEST_CASE("hiding lean: beyond 8 cm, held to 5 cm, full at 25 cm") {
    HidingLean lean;
    BodyPoses p = standing();
    HidingLean::Input in{.hiding = true, .menu = false, .open = true, .poses = &p};
    auto r = lean.update(true, in);
    CHECK(r.reference_taken);
    CHECK(r.move.x == 0.0f);

    p.head.x = 0.06f; // inside the start distance
    CHECK(lean.update(true, in).move.x == 0.0f);
    p.head.x = 0.10f;
    CHECK(lean.update(true, in).move.x == doctest::Approx(0.4f));
    p.head.x = 0.06f; // still leaning: above the stop distance
    CHECK(lean.update(true, in).move.x == doctest::Approx(0.24f));
    p.head.x = 0.04f;
    CHECK(lean.update(true, in).move.x == 0.0f);
    p.head.x = -0.40f;
    CHECK(lean.update(true, in).move.x == doctest::Approx(-1.0f));
    p.head.x = 0.0f;
    CHECK(lean.update(true, in).move.x == 0.0f);

    // Forward and back on the forward axis; back leans back (stick down).
    p.head.z = 0.125f;
    CHECK(lean.update(true, in).move.y == doctest::Approx(-0.5f));
    p.head.z = -0.30f;
    CHECK(lean.update(true, in).move.y == doctest::Approx(1.0f));
    p.head.z = 0.0f;
    CHECK(lean.update(true, in).move.y == 0.0f);
    // A rise peeks out as a forward lean; a duck is none.
    p.head.y = 1.70f;
    CHECK(lean.update(true, in).move.y == doctest::Approx(0.4f));
    p.head.y = 1.40f;
    CHECK(lean.update(true, in).move.y == 0.0f);
}

TEST_CASE("hiding lean: relative to the head when hiding began, in its frame") {
    HidingLean lean;
    BodyPoses p = standing();
    p.head      = {2.0f, 1.2f, 3.0f};
    p.head_yaw  = kQuarterTurn; // facing -X: right is -Z
    HidingLean::Input in{.hiding = true, .open = true, .poses = &p};
    lean.update(true, in);
    p.head_yaw = 0.0f;         // turning the head later changes nothing
    p.head.z   = 3.0f - 0.25f; // 25 cm to the right of the reference
    CHECK(lean.update(true, in).move.x == doctest::Approx(1.0f));

    // A menu: no lean, the reference kept.
    in.menu = true;
    CHECK(lean.update(true, in).move.x == 0.0f);
    in.menu = false;
    CHECK(lean.update(true, in).move.x == doctest::Approx(1.0f));

    // Leaving the hiding place drops the reference; the next one is new.
    in.hiding = false;
    CHECK(lean.update(true, in).move.x == 0.0f);
    in.hiding = true;
    CHECK(lean.update(true, in).reference_taken);
    CHECK(lean.update(true, in).move.x == 0.0f);

    // Off: nothing.
    HidingLean off;
    p.head.x = 5.0f;
    CHECK(off.update(false, in).move.x == 0.0f);
}

// ---- 22 §9.3 -------------------------------------------------------------------

TEST_CASE("hold breath: a hand within 12 cm of the mouth, released beyond 18 cm") {
    HoldBreath breath;
    BodyPoses p = standing();
    HoldBreath::Input in{.breathing_game = true, .open = true, .poses = &p};
    // The mouth point: 9 cm below and 7 cm in front of the head.
    place(p, kRightHand, {0.0f, -0.09f, -0.07f - 0.15f});
    CHECK_FALSE(breath.update(true, in)); // 15 cm: not yet
    place(p, kRightHand, {0.0f, -0.09f, -0.07f - 0.10f});
    CHECK(breath.update(true, in));
    place(p, kRightHand, {0.0f, -0.09f, -0.07f - 0.17f});
    CHECK(breath.update(true, in)); // held until beyond 18 cm
    place(p, kRightHand, {0.0f, -0.09f, -0.07f - 0.19f});
    CHECK_FALSE(breath.update(true, in));

    // Either hand; only during the breathing game.
    place(p, kLeftHand, {0.05f, -0.09f, -0.07f});
    CHECK(breath.update(true, in));
    in.breathing_game = false;
    CHECK_FALSE(breath.update(true, in));
    in.breathing_game = true;
    CHECK_FALSE(breath.update(false, in)); // off
    p.hand_valid[kLeftHand] = false;
    CHECK_FALSE(breath.update(true, in));
}

// ---- the set and the control mapper -------------------------------------------

namespace {

struct Rig {
    Glyphs glyphs;
    RecordingLog log;
    VirtualPad pad{glyphs, log};
    ControlSchemes schemes{glyphs};
    omvr::core::config::InputSettings input;
    omvr::core::config::ControlsSettings controls;
    FreeStick stick;
    FakeClock clock;
    FakeContext context;
    Gestures gestures{{controls, context, log}};
    ControlMapper mapper{{pad, schemes, input, controls, stick, clock, log, &gestures}};

    const ActionTier& player() { return pad.tiers()[game::kPlayerMap]; }

    // Writes `c` after the focus gate has opened.
    void settle(const Controls& c) {
        mapper.write(c, false);
        clock.advance(600ms);
        mapper.write(c, false);
    }
};

} // namespace

TEST_CASE("control mapper: with every gesture off the grips are v1's") {
    Rig rig;
    Controls c;
    c.poses  = standing();
    c.grip_r = true;
    place(c.poses, kRightHand, {0.15f, -0.1f, 0.15f}); // at the right shoulder
    rig.settle(c);
    CHECK(rig.player().digital[pa::kAim] == 1);
    CHECK(rig.player().digital[pa::kNextWeapon] == 0);
}

TEST_CASE("control mapper: a holster press drives its action, not Aim") {
    Rig rig;
    rig.controls.holster_gestures = true;
    Controls c;
    c.poses = standing();
    place(c.poses, kRightHand, {0.15f, -0.1f, 0.15f});
    rig.settle(c); // no grip yet
    c.grip_r = true;
    rig.mapper.write(c, false);
    CHECK(rig.player().digital[pa::kNextWeapon] == 1);
    CHECK(rig.player().digital[pa::kAim] == 0);
    CHECK(rig.log.contains("holster right shoulder (right hand): next_main_weapon"));
    c.grip_r = false;
    rig.mapper.write(c, false);
    CHECK(rig.player().digital[pa::kNextWeapon] == 0);

    // The same squeeze in front of the body aims as v1.
    place(c.poses, kRightHand, {0.2f, -0.3f, -0.4f});
    c.grip_r = true;
    rig.mapper.write(c, false);
    CHECK(rig.player().digital[pa::kAim] == 1);
    CHECK(rig.player().digital[pa::kNextWeapon] == 0);

    // A consumed press is not counted by Input.ToggleGrips.
    c.grip_r = false;
    rig.mapper.write(c, false);
    rig.input.toggle_grips = true;
    place(c.poses, kRightHand, {0.15f, -0.1f, 0.15f});
    c.grip_r = true;
    rig.mapper.write(c, false);
    c.grip_r = false;
    rig.mapper.write(c, false);
    CHECK(rig.player().digital[pa::kAim] == 0);

    // The left hip is the flashlight, the chest the tracker.
    rig.input.toggle_grips = false;
    place(c.poses, kLeftHand, {-0.25f, -0.65f, 0.0f});
    c.grip_l = true;
    rig.mapper.write(c, false);
    CHECK(rig.player().digital[pa::kFlashlight] == 1);
    c.grip_l = false;
    rig.mapper.write(c, false);
    place(c.poses, kLeftHand, {0.0f, -0.30f, -0.08f});
    c.grip_l = true;
    rig.mapper.write(c, false);
    CHECK(rig.player().digital[pa::kTracker] == 1);
    CHECK(rig.player().digital[pa::kFlashlight] == 0);

    // An interaction: v1.
    c.grip_l = false;
    rig.mapper.write(c, false);
    rig.context.state.interaction = true;
    place(c.poses, kRightHand, {0.15f, -0.1f, 0.15f});
    c.grip_r = true;
    rig.mapper.write(c, false);
    CHECK(rig.player().digital[pa::kNextWeapon] == 0);
    CHECK(rig.player().digital[pa::kAim] == 1);
}

TEST_CASE("control mapper: the recenter gesture asks the game thread once") {
    Rig rig;
    rig.controls.recenter_gesture = true;
    Controls c;
    c.poses = standing();
    place(c.poses, kLeftHand, {-0.1f, -0.05f, -0.15f});
    place(c.poses, kRightHand, {0.1f, -0.05f, -0.15f});
    c.grip_l = c.grip_r = true;
    rig.settle(c); // 0.6 s held, but the hold began before the focus gate opened
    CHECK_FALSE(rig.gestures.take_recenter());
    rig.clock.advance(749ms);
    rig.mapper.write(c, false);
    CHECK_FALSE(rig.gestures.take_recenter());
    rig.clock.advance(1ms);
    rig.mapper.write(c, false);
    CHECK(rig.gestures.take_recenter());
    CHECK_FALSE(rig.gestures.take_recenter());
    CHECK(rig.log.contains("gesture: recenter"));
    // The grips still drive Aim and the tracker while it is held.
    CHECK(rig.player().digital[pa::kAim] == 1);
    CHECK(rig.player().digital[pa::kTracker] == 1);

    // Not with a hand beyond 0.30 m.
    Rig far;
    far.controls.recenter_gesture = true;
    Controls d                    = c;
    place(d.poses, kRightHand, {0.25f, -0.15f, -0.15f}); // 0.33 m
    far.settle(d);
    far.clock.advance(2s);
    far.mapper.write(d, false);
    CHECK_FALSE(far.gestures.take_recenter());
}

TEST_CASE("control mapper: lean and hold breath while hiding") {
    Rig rig;
    rig.controls.hiding_lean         = true;
    rig.controls.hold_breath_gesture = true;
    rig.context.state.hiding         = true;
    Controls c;
    c.poses = standing();
    c.move  = {0.3f, 0.0f};
    rig.settle(c);
    CHECK(rig.player().analog[pa::kMoveX] == doctest::Approx(0.3f));
    c.poses.head.x = 0.1f; // 0.4 of a full lean, added to the stick
    rig.mapper.write(c, false);
    CHECK(rig.player().analog[pa::kMoveX] == doctest::Approx(0.7f));
    c.poses.head.x = 0.3f; // full, clamped with the stick
    rig.mapper.write(c, false);
    CHECK(rig.player().analog[pa::kMoveX] == doctest::Approx(1.0f));
    CHECK(rig.log.contains("hiding lean ("));

    // The breathing game: a hand over the mouth presses the hold-breath input.
    rig.context.state.breathing_game = true;
    place(c.poses, kRightHand, {0.0f, -0.09f, -0.07f});
    rig.mapper.write(c, false);
    CHECK(rig.player().digital[pa::kAim] == 1);
    CHECK(rig.log.contains("hold breath (hand over the mouth)"));
    rig.context.state.breathing_game = false;
    rig.mapper.write(c, false);
    CHECK(rig.player().digital[pa::kAim] == 0);
}

TEST_CASE("control mapper: no gesture while the runtime has no focus") {
    Rig rig;
    rig.controls.holster_gestures = true;
    Controls c;
    c.poses = standing();
    place(c.poses, kRightHand, {0.15f, -0.1f, 0.15f});
    c.poses.focused = false;
    c.grip_r        = true;
    rig.mapper.write(c, false);
    CHECK(rig.player().digital[pa::kNextWeapon] == 0);
    // Focus back: a press within 0.5 s is v1's.
    c.grip_r        = false;
    c.poses.focused = true;
    rig.mapper.write(c, false);
    rig.clock.advance(300ms);
    c.grip_r = true;
    rig.mapper.write(c, false);
    CHECK(rig.player().digital[pa::kNextWeapon] == 0);
    CHECK(rig.player().digital[pa::kAim] == 1);
}
