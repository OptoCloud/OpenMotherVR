// OpenMotherVR-Provenance: clean
// core/stereo comfort (spec 22 §4.3, §4.4, §5): the vignette's shape and
// ramp, the snap-turn fade, the render thread's per-frame decision, and
// interaction-aware turning in the stereo update. Expected values are the
// spec's numbers.

#include "core/stereo/comfort_view.h"
#include "core/stereo/scene_tracker.h"
#include "core/stereo/stereo_update.h"
#include "core/stereo/turn_block.h"
#include "core/update_input.h"
#include "fakes/fakes.h"
#include "util/math/angle.h"

#include <doctest/doctest.h>

#include <chrono>
#include <cmath>

namespace {

using namespace omvr::core::stereo;
using omvr::core::config::Settings;
using omvr::core::input::Interaction;
using omvr::tests::pad_input;
using omvr::tests::RecordingLog;
using namespace std::chrono_literals;
using Time = std::chrono::steady_clock::time_point;

constexpr float kDeg = omvr::util::kDegToRad;

} // namespace

TEST_CASE("vignette radii: 60 - 35k degrees inside, 15 degrees of falloff") {
    const VignetteRadii full = vignette_radii(1.0f);
    CHECK(full.inner_rad == doctest::Approx(25.0f * kDeg));
    CHECK(full.outer_rad == doctest::Approx(40.0f * kDeg));
    const VignetteRadii half = vignette_radii(0.5f);
    CHECK(half.inner_rad == doctest::Approx(42.5f * kDeg));
    CHECK(half.outer_rad == doctest::Approx(57.5f * kDeg));
    // Off: no pixel (always under 90 degrees) is darkened.
    const VignetteRadii off = vignette_radii(0.0f);
    CHECK(vignette_factor(89.0f * kDeg, off) == 1.0f);
}

TEST_CASE("vignette factor: untouched inside, black outside, smooth between") {
    const VignetteRadii r = vignette_radii(1.0f);
    CHECK(vignette_factor(0.0f, r) == 1.0f);
    CHECK(vignette_factor(24.9f * kDeg, r) == 1.0f);
    CHECK(vignette_factor(40.1f * kDeg, r) == 0.0f);
    CHECK(vignette_factor(32.5f * kDeg, r) == doctest::Approx(0.5f));
    const float a = vignette_factor(28.0f * kDeg, r);
    const float b = vignette_factor(36.0f * kDeg, r);
    CHECK(a > b);
    CHECK(a < 1.0f);
    CHECK(b > 0.0f);
}

TEST_CASE("vignette angle: from the centre of the eye half by the FOV tangents") {
    // The centre of the half is the forward axis.
    CHECK(vignette_angle(0.5f, 0.5f, 1.0f, 1.0f) == doctest::Approx(0.0f));
    // The horizontal edge at tangent 1 is 45 degrees out, either side.
    CHECK(vignette_angle(1.0f, 0.5f, 1.0f, 1.2f) == doctest::Approx(45.0f * kDeg));
    CHECK(vignette_angle(0.0f, 0.5f, 1.0f, 1.2f) == doctest::Approx(45.0f * kDeg));
    // A corner combines both tangents: atan(sqrt(1 + 1)).
    CHECK(vignette_angle(0.0f, 0.0f, 1.0f, 1.0f) == doctest::Approx(std::atan(std::sqrt(2.0f))));
    // The same in both eyes: the angle depends on the position in the half.
    CHECK(vignette_angle(0.25f, 0.75f, 1.0f, 1.0f) == vignette_angle(0.25f, 0.75f, 1.0f, 1.0f));
}

TEST_CASE("vignette ramp: 0 to full in 150 ms, full to 0 in 300 ms") {
    VignetteRamp ramp;
    CHECK(ramp.advance(1.0f, 75ms) == doctest::Approx(0.5f));
    CHECK(ramp.advance(1.0f, 75ms) == doctest::Approx(1.0f));
    CHECK(ramp.advance(1.0f, 1s) == doctest::Approx(1.0f));
    CHECK(ramp.advance(0.0f, 150ms) == doctest::Approx(0.5f));
    CHECK(ramp.advance(0.0f, 150ms) == doctest::Approx(0.0f));
    CHECK(ramp.advance(0.0f, 1s) == 0.0f);
    // The same times at a lower setting.
    CHECK(ramp.advance(0.4f, 150ms) == doctest::Approx(0.4f));
    CHECK(ramp.advance(0.0f, 150ms) == doctest::Approx(0.2f));
}

TEST_CASE("snap fade: black on the step, clear after 100 ms, restarts on a step") {
    SnapFade fade;
    const Time t0{1s};
    CHECK(fade.darkness(0, t0) == 0.0f);
    CHECK(fade.darkness(1, t0) == 1.0f);
    CHECK(fade.darkness(1, t0 + 50ms) == doctest::Approx(0.5f));
    // A further step during the fade-in restarts it from black.
    CHECK(fade.darkness(2, t0 + 60ms) == 1.0f);
    CHECK(fade.darkness(2, t0 + 110ms) == doctest::Approx(0.5f));
    CHECK(fade.darkness(2, t0 + 160ms) == 0.0f);
    CHECK(fade.darkness(2, t0 + 1s) == 0.0f);
}

TEST_CASE("comfort view: nothing to draw while off, in screen frames, or settled") {
    ComfortView view;
    const Time t0{1s};
    // Off: the target is 0 and no step was made.
    CHECK_FALSE(view.frame(t0, true, 1.0f, 1.0f).has_value());

    view.publish_target(1.0f);
    // Screen frames are never darkened.
    CHECK_FALSE(view.frame(t0 + 20ms, false, 1.0f, 1.0f).has_value());
    // Head-tracked: ramps up from 0, half way after 75 ms.
    auto c = view.frame(t0 + 95ms, true, 1.0f, 1.2f);
    REQUIRE(c.has_value());
    CHECK(c->tan_h == 1.0f);
    CHECK(c->tan_v == 1.2f);
    CHECK(c->brightness == 1.0f);
    CHECK(c->inner_rad == doctest::Approx((60.0f - 35.0f * 0.5f) * kDeg));

    // A snap step: the frame is black, the vignette goes on.
    view.note_snap_step();
    c = view.frame(t0 + 100ms, true, 1.0f, 1.0f);
    REQUIRE(c.has_value());
    CHECK(c->brightness == 0.0f);

    // Stopped: back to nothing once the strength has fallen and the fade ended.
    view.publish_target(0.0f);
    CHECK(view.frame(t0 + 150ms, true, 1.0f, 1.0f).has_value());
    CHECK_FALSE(view.frame(t0 + 600ms, true, 1.0f, 1.0f).has_value());
}

TEST_CASE("comfort view: a fade from black needs no vignette") {
    ComfortView view;
    const Time t0{1s};
    CHECK_FALSE(view.frame(t0, true, 1.0f, 1.0f).has_value());
    view.note_snap_step();
    const auto c = view.frame(t0 + 10ms, true, 1.0f, 1.0f);
    REQUIRE(c.has_value());
    CHECK(c->brightness == 0.0f);
    // The vignette part leaves every pixel untouched.
    CHECK(vignette_factor(89.0f * kDeg, {c->inner_rad, c->outer_rad}) == 1.0f);
}

TEST_CASE("stereo update: vignette target from stick movement and turning") {
    Settings s;
    s.comfort.vignette    = 0.8f;
    s.input.snap_rotation = false;
    SceneTracker scene;
    StereoUpdate update(s, scene);
    const Time t0{1s};

    SUBCASE("movement above 0.1 drives it, below does not") {
        CHECK(update.run(pad_input(t0, 0.0f, 0.0f, 0.05f)).vignette_target == 0.0f);
        CHECK(update.run(pad_input(t0 + 10ms, 0.0f, 0.0f, 0.5f)).vignette_target == 0.8f);
    }
    SUBCASE("the game's own look turning with the stick, past 0.33") {
        CHECK(update.run(pad_input(t0, 0.3f)).vignette_target == 0.0f);
        CHECK(update.run(pad_input(t0 + 10ms, 0.6f)).vignette_target == 0.8f);
    }
    SUBCASE("smooth turn changing the yaw") {
        s.comfort.smooth_turn_speed = 90.0f;
        CHECK(update.run(pad_input(t0, 1.0f)).vignette_target == 0.0f); // the first update turns by nothing
        CHECK(update.run(pad_input(t0 + 10ms, 1.0f)).vignette_target == 0.8f);
    }
    SUBCASE("snap steps do not drive it; they ask for the fade when it is on") {
        s.input.snap_rotation    = true;
        s.comfort.snap_turn_fade = true;
        const UpdateOutput out   = update.run(pad_input(t0, 1.0f));
        CHECK(out.yaw.has_value());
        CHECK(out.snap_fade);
        CHECK(out.vignette_target == 0.0f);
    }
    SUBCASE("no fade with the setting off") {
        s.input.snap_rotation = true;
        CHECK_FALSE(update.run(pad_input(t0, 1.0f)).snap_fade);
    }
    SUBCASE("never in the menu screen or without a camera") {
        UpdateInput menu = pad_input(t0, 0.0f, 0.0f, 1.0f);
        menu.mode        = StereoMode::Screen;
        CHECK(update.run(menu).vignette_target == 0.0f);
        UpdateInput cutscene   = pad_input(t0 + 10ms, 0.0f, 0.0f, 1.0f);
        cutscene.follow_active = false;
        CHECK(update.run(cutscene).vignette_target == 0.0f);
    }
}

TEST_CASE("stereo update: interaction-aware turning blocks snap, smooth and game look") {
    Settings s;
    SceneTracker scene;
    StereoUpdate update(s, scene);
    const Time t0{1s};
    UpdateInput keypad      = pad_input(t0, 1.0f);
    keypad.in_interaction   = true;
    keypad.interaction_kind = Interaction::Keypad;

    SUBCASE("off: the interaction state is ignored (v1)") { CHECK(update.run(keypad).yaw.has_value()); }
    SUBCASE("snap: no step or repeat while blocked; the stick must come back first") {
        s.controls.interaction_aware_turning = true;
        const UpdateOutput first             = update.run(keypad);
        CHECK_FALSE(first.yaw.has_value());
        REQUIRE(first.block_changed.has_value());
        CHECK(first.block_changed->block == TurnBlock::Interaction);
        CHECK(first.block_changed->kind == Interaction::Keypad);
        keypad.now = t0 + 1s;
        CHECK_FALSE(update.run(keypad).yaw.has_value());
        // The block ends with the stick still held: no step, and no repeat.
        UpdateOutput out = update.run(pad_input(t0 + 2s, 1.0f));
        CHECK_FALSE(out.yaw.has_value());
        REQUIRE(out.block_changed.has_value());
        CHECK(out.block_changed->block == TurnBlock::None);
        CHECK_FALSE(update.run(pad_input(t0 + 3s, 1.0f)).yaw.has_value());
        CHECK_FALSE(update.run(pad_input(t0 + 4s, 1.0f)).yaw.has_value());
        // Released inside 0.5, then the next flick steps.
        CHECK_FALSE(update.run(pad_input(t0 + 5s, 0.0f)).yaw.has_value());
        CHECK(update.run(pad_input(t0 + 5s + 10ms, 1.0f)).yaw.has_value());
    }
    SUBCASE("smooth: stops, then resumes with no step") {
        s.controls.interaction_aware_turning = true;
        s.input.snap_rotation                = false;
        s.comfort.smooth_turn_speed          = 90.0f;
        CHECK_FALSE(update.run(pad_input(t0, 1.0f)).yaw.has_value());
        CHECK(update.run(pad_input(t0 + 10ms, 1.0f)).yaw.has_value());
        keypad.now = t0 + 20ms;
        CHECK_FALSE(update.run(keypad).yaw.has_value());
        CHECK_FALSE(update.run(pad_input(t0 + 30ms, 1.0f)).yaw.has_value());
        CHECK(update.run(pad_input(t0 + 40ms, 1.0f)).yaw.has_value());
    }
    SUBCASE("game look: the look byte stays off for the pad, on for the mouse") {
        s.controls.interaction_aware_turning = true;
        s.input.snap_rotation                = false;
        CHECK(update.run(pad_input(t0, 0.0f)).look_enabled == true);
        keypad.now             = t0 + 10ms;
        const UpdateOutput out = update.run(keypad);
        CHECK(out.look_enabled == false);
        CHECK(out.vignette_target == 0.0f);
        UpdateInput mouse           = keypad;
        mouse.now                   = t0 + 20ms;
        mouse.pad_activity          = false;
        mouse.keyboard_and_mouse    = true;
        mouse.key_or_mouse_activity = true;
        CHECK(update.run(mouse).look_enabled == true);
    }
    SUBCASE("the breathing game and cutscenes block; hiding alone does not") {
        s.controls.interaction_aware_turning = true;
        UpdateInput breathing                = pad_input(t0, 1.0f);
        breathing.breathing_game             = true;
        const UpdateOutput out               = update.run(breathing);
        CHECK_FALSE(out.yaw.has_value());
        REQUIRE(out.block_changed.has_value());
        CHECK(out.block_changed->block == TurnBlock::BreathingGame);

        CHECK(turn_block({.cutscene_or_fade = true}) == TurnBlock::Cutscene);
        CHECK(turn_block({}) == TurnBlock::None);
    }
}

TEST_CASE("stereo update: the turning in use is reported once per change") {
    Settings s;
    SceneTracker scene;
    StereoUpdate update(s, scene);
    const Time t0{1s};
    UpdateOutput out = update.run(pad_input(t0, 0.0f));
    REQUIRE(out.turning_changed.has_value());
    CHECK(out.turning_changed->turning == Turning::Snap);
    CHECK(out.turning_changed->repeat_ms == 300);
    CHECK_FALSE(update.run(pad_input(t0 + 10ms, 0.0f)).turning_changed.has_value());

    s.input.snap_rotation       = false;
    s.comfort.smooth_turn_speed = 90.0f;
    out                         = update.run(pad_input(t0 + 20ms, 0.0f));
    REQUIRE(out.turning_changed.has_value());
    CHECK(out.turning_changed->turning == Turning::Smooth);
    CHECK(out.turning_changed->speed_deg_s == 90.0f);

    s.comfort.smooth_turn_speed = 0.0f;
    out                         = update.run(pad_input(t0 + 30ms, 0.0f));
    REQUIRE(out.turning_changed.has_value());
    CHECK(out.turning_changed->turning == Turning::GameLook);
}

TEST_CASE("breathing game state from the hiding place's events") {
    RecordingLog log;
    omvr::core::input::InteractionState s(log);
    CHECK_FALSE(s.breathing_game());
    CHECK(s.on_event("Start_Breathing_Game"));
    CHECK(s.breathing_game());
    CHECK(s.on_event("End_Breathing_Game"));
    CHECK_FALSE(s.breathing_game());
    // Also cleared on leaving the hiding place and at a loading screen.
    s.on_event("Start_Breathing_Game");
    s.on_event("hiding_visibility_finish");
    CHECK_FALSE(s.breathing_game());
    s.on_event("Start_Breathing_Game");
    s.on_loading_screen();
    CHECK_FALSE(s.breathing_game());
}

TEST_CASE("turning log lines") {
    RecordingLog log;
    log_turning(log, {.turning = Turning::Smooth, .speed_deg_s = 90.0f});
    log_turning(log, {.turning = Turning::Snap, .repeat_ms = 0});
    REQUIRE(log.lines.size() == 2);
    CHECK(log.lines[0].find("smooth") != std::string::npos);
    CHECK(log.lines[0].find("90") != std::string::npos);
    CHECK(log.lines[1].find("one step per push") != std::string::npos);
    // The block line only with Dev. Debug Options.
    log_turn_block(log, {.block = TurnBlock::Interaction, .kind = Interaction::Terminal});
    CHECK(log.lines.size() == 2);
    log.debug = true;
    log_turn_block(log, {.block = TurnBlock::Interaction, .kind = Interaction::Terminal});
    REQUIRE(log.lines.size() == 3);
    CHECK(log.lines[2].find("terminal") != std::string::npos);
}
