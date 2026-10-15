// core/stereo: render target size and its cap, the mirror layout, snap
// turn, UI quad placement, the mode policy, the scene tracker, the
// head-clip delay and the stereo AA decisions.

#include "core/stereo/head_clip_delay.h"
#include "core/stereo/mirror_layout.h"
#include "core/stereo/mode_policy.h"
#include "core/stereo/render_target.h"
#include "core/stereo/scene_tracker.h"
#include "core/stereo/snap_turn.h"
#include "core/stereo/stereo_aa.h"
#include "core/stereo/tuning.h"
#include "core/stereo/ui_placement.h"
#include "fakes/fakes.h"
#include "util/math/matrix.h"

#include <doctest/doctest.h>

#include <chrono>
#include <optional>

namespace {

using namespace omvr::core::stereo;
using omvr::core::config::MirrorViewType;
using omvr::core::vr::FovTangents;
using omvr::core::vr::TextureSize;
using omvr::tests::FakeClock;
using omvr::tests::RecordingLog;
using namespace std::chrono_literals;

TEST_CASE("pixel scale is capped so the texture stays within 16384") {
    CHECK(capped_pixel_scale(2.0f, {4000, 4000}, {4000, 4000}) == 2.0f);
    CHECK(capped_pixel_scale(1.0f, {9000, 2000}, {9000, 2000}) == doctest::Approx(16384.0f / 18000.0f));
    CHECK(capped_pixel_scale(1.0f, {1000, 20000}, {1000, 10000}) == doctest::Approx(16384.0f / 20000.0f));
    CHECK(capped_pixel_scale(3.0f, {0, 0}, {0, 0}) == 3.0f);  // no runtime sizes yet
}

TEST_CASE("side-by-side target and eye viewports") {
    const PixelSize s = side_by_side({100, 200}, {110, 190});
    CHECK(s.width == 210);
    CHECK(s.height == 200);

    const EyeViewports v = eye_viewports({201, 100});
    CHECK(v[0].x == 0);
    CHECK(v[0].width == 100);
    CHECK(v[1].x == 100);
    CHECK(v[1].width == 100);
    CHECK(v[1].height == 100);

    CHECK(layout_follows({10, 10}, {0, 0}));
    CHECK(layout_follows({10, 10}, {10, 10}));
    CHECK_FALSE(layout_follows({10, 10}, {20, 10}));
}

MirrorView view_of(MirrorViewType type, bool symmetric) {
    const FovTangents unit{1.0f, 1.0f, 1.0f, 1.0f};
    return {.texture      = {3200, 1600},
            .window       = {1600, 900},
            .real_fov     = {unit, unit},
            .rendered_fov = {unit, unit},
            .type         = type,
            .symmetric    = symmetric};
}

void check_panel(const MirrorConstants& c, int panel, std::array<float, 4> draw, std::array<float, 4> source) {
    for (int i = 0; i < 4; ++i) {
        CHECK(c.draw[panel][i] == doctest::Approx(draw[static_cast<std::size_t>(i)]));
        CHECK(c.source[panel][i] == doctest::Approx(source[static_cast<std::size_t>(i)]));
    }
}

TEST_CASE("mirror layout: screen frames fit the screen region whole") {
    const MirrorConstants c = mirror_constants(view_of(MirrorViewType::RightEye, true), true);
    // Region 0.5 x 0.5 of 3200x1600 is 2:1, wider than 16:9: bars above
    // and below.
    const float k = (1600.0f / 900.0f) / 2.0f;
    check_panel(c, 0, {0.0f, (1 - k) / 2, 1.0f, (1 + k) / 2}, {0.0f, 0.25f, 0.5f, 0.75f});
    check_panel(c, 1, {0, 0, 0, 0}, {0, 0, 0, 0});

    MirrorView wide = view_of(MirrorViewType::RightEye, true);
    wide.window = {2000, 900};  // wider than 2:1: bars at the sides
    const MirrorConstants t = mirror_constants(wide, true);
    const float j = 2.0f / (2000.0f / 900.0f);
    CHECK(t.draw[0][0] == doctest::Approx((1 - j) / 2));
    CHECK(t.draw[0][1] == 0.0f);
    CHECK(t.draw[0][3] == 1.0f);
}

TEST_CASE("mirror layout: symmetric eyes") {
    const float window = 1600.0f / 900.0f;
    {
        const MirrorConstants c = mirror_constants(view_of(MirrorViewType::RightEye, true), false);
        const float k = window / 1.0f;  // half of 3200x1600 is square
        check_panel(c, 0, {0.0f, (1 - k) / 2, 1.0f, (1 + k) / 2}, {0.5f, 0.0f, 1.0f, 1.0f});
    }
    {
        const MirrorConstants c = mirror_constants(view_of(MirrorViewType::LeftEye, true), false);
        CHECK(c.source[0][0] == 0.0f);
        CHECK(c.source[0][2] == 0.5f);
    }
    {
        const MirrorConstants c = mirror_constants(view_of(MirrorViewType::BothEyes, true), false);
        const float k = window / 2.0f;
        check_panel(c, 0, {0.0f, (1 - k) / 2, 1.0f, (1 + k) / 2}, {0.0f, 0.0f, 1.0f, 1.0f});
    }
}

TEST_CASE("mirror layout: both eyes cropped to their real FOV") {
    MirrorView v = view_of(MirrorViewType::BothEyes, false);
    v.real_fov[0] = {0.5f, 1.0f, 1.0f, 0.5f};  // up, down, left, right
    v.real_fov[1] = {1.0f, 0.5f, 0.5f, 1.0f};
    const MirrorConstants c = mirror_constants(v, false);
    const float k = (1600.0f / 900.0f) / 1.5f;  // 0.75 of the width, all rows
    const float top = (1 - k) / 2, bottom = (1 + k) / 2;
    check_panel(c, 0, {0.0f, top, 0.5f, bottom}, {0.0f, 0.0f, 0.375f, 1.0f});
    check_panel(c, 1, {0.5f, top, 1.0f, bottom}, {0.625f, 0.0f, 1.0f, 1.0f});
}

TEST_CASE("mirror layout: an unknown window takes the texture's aspect") {
    MirrorView v = view_of(MirrorViewType::BothEyes, true);
    v.window = {0, 0};
    const MirrorConstants c = mirror_constants(v, false);
    CHECK(c.draw[0][1] == doctest::Approx(0.0f));
    CHECK(c.draw[0][3] == doctest::Approx(1.0f));
}

TEST_CASE("snap turn steps and wraps") {
    CHECK(turn_out(0.6f));
    CHECK(turn_out(-0.6f));
    CHECK_FALSE(turn_out(kTurnThreshold));
    CHECK_FALSE(turn_out(0.0f));

    CHECK(snap_step(2) == 1.0f);
    CHECK(snap_step(3) == doctest::Approx(2.0f / 3.0f));
    CHECK(snap_step(12) == doctest::Approx(2.0f / 12.0f));
    CHECK(snap_step(24) == doctest::Approx(2.0f / 24.0f));
    CHECK(snap_step(1) == 1.0f);  // at least two segments

    CHECK(snapped_yaw(0.0f, 1.0f, 12) == doctest::Approx(1.0f / 6.0f));
    CHECK(snapped_yaw(0.0f, -1.0f, 12) == doctest::Approx(-1.0f / 6.0f));
    CHECK(snapped_yaw(0.9f, 1.0f, 12) == doctest::Approx(0.9f + 1.0f / 6.0f - 2.0f));
    CHECK(snapped_yaw(-0.9f, -1.0f, 12) == doctest::Approx(-0.9f - 1.0f / 6.0f + 2.0f));
    CHECK(snapped_yaw(0.5f, 1.0f, 2) == doctest::Approx(-0.5f));
}

TEST_CASE("virtual screen and HUD quads follow the view") {
    CHECK(virtual_screen_quad(std::nullopt, 0.5f)[0] == 1.0f);  // identity without a view

    Matrix44 identity{};
    omvr::util::identity(identity.data());
    const Matrix44 screen = virtual_screen_quad(identity, kMenuScreenInvAspect);
    CHECK(screen[0] == 1.0f);   // right
    CHECK(screen[5] == doctest::Approx(kMenuScreenInvAspect));  // up scaled
    CHECK(screen[10] == 1.0f);  // forward
    CHECK(screen[12] == 0.0f);
    CHECK(screen[14] == doctest::Approx(kMenuScreenDistance));
    CHECK(screen[15] == 1.0f);

    const Matrix44 hud = hud_panel_quad(identity, 0.5f);
    CHECK(hud[0] == doctest::Approx(kHudPanelWidth));
    CHECK(hud[5] == doctest::Approx(kHudPanelWidth * 0.5f));
    CHECK(hud[10] == 1.0f);
    CHECK(hud[14] == 1.0f);

    // Looking straight down: the screen faces +Z.
    Matrix44 down{};
    omvr::util::identity(down.data());
    down[2]  = 0.0f;
    down[10] = 0.0f;
    down[6]  = -1.0f;
    const Matrix44 d = virtual_screen_quad(down, 1.0f);
    CHECK(d[8] == 0.0f);
    CHECK(d[10] == 1.0f);
}

TEST_CASE("mode policy: push, pop, enter and leave screen mode") {
    const ModeSteps open = ModePolicy::push(StereoMode::World, StereoMode::Screen);
    CHECK_FALSE(open.pop);
    CHECK(open.push == StereoMode::Screen);

    const ModeSteps again = ModePolicy::push(StereoMode::Screen, StereoMode::Screen);
    CHECK(again.pop);
    CHECK(again.push == StereoMode::Screen);

    CHECK(ModePolicy::pop(StereoMode::Screen).pop_to_world);
    CHECK_FALSE(ModePolicy::pop(StereoMode::World).pop_to_world);

    const ModeSteps enter = ModePolicy::enter_screen_mode(StereoMode::World);
    CHECK_FALSE(enter.pop);
    CHECK(enter.push == StereoMode::Screen);
    CHECK(ModePolicy::enter_screen_mode(StereoMode::Screen).pop);
    CHECK(ModePolicy::leave_screen_mode(StereoMode::Screen).pop_to_world);
    CHECK_FALSE(ModePolicy::leave_screen_mode(StereoMode::World).pop_to_world);
}

TEST_CASE("mode policy: the stack count follows the mode") {
    using S = StackSync;
    CHECK(ModePolicy::sync_mode_stack({StereoMode::World, 1, 4, true}, true) == S::Lower);
    CHECK(ModePolicy::sync_mode_stack({StereoMode::Screen, 2, 4, true}, true) == S::Lower);
    CHECK(ModePolicy::sync_mode_stack({StereoMode::Screen, 1, 4, true}, true) == S::None);
    CHECK(ModePolicy::sync_mode_stack({StereoMode::World, 0, 0, false}, false) == S::None);
    CHECK(ModePolicy::sync_mode_stack({StereoMode::Screen, 0, 0, false}, true) == S::Append);
    CHECK(ModePolicy::sync_mode_stack({StereoMode::Screen, 0, 1, true}, false) == S::WriteInPlace);
    CHECK(ModePolicy::sync_mode_stack({StereoMode::Screen, 0, 0, true}, false) == S::NoStorage);
    CHECK(ModePolicy::sync_mode_stack({StereoMode::Screen, 0, 4, false}, false) == S::NoStorage);
}

TEST_CASE("mode policy reports each failure once") {
    RecordingLog log;
    ModePolicy policy(log);
    policy.report(StackSyncOutcome::Done);
    policy.report(StackSyncOutcome::AppendFailed);
    policy.report(StackSyncOutcome::AppendFailed);
    policy.report(StackSyncOutcome::NoStorage);
    policy.report(StackSyncOutcome::NoStorage);
    REQUIRE(log.lines.size() == 2);
    CHECK(log.lines[0] == "[stereo] the game's mode-stack append failed; count left at 0");
    CHECK(log.lines[1] ==
          "[stereo] mode 1 entered without a push and the mode stack has no storage; count left at 0");
}

TEST_CASE("scene tracker keeps the scene visible through a cutscene") {
    const SceneMessages names{1, 2, 3, 4};
    SceneTracker t;
    CHECK_FALSE(t.update(true, StereoMode::World).has_value());

    t.on_message(1, names);  // cutscene starts
    CHECK(t.update(true, StereoMode::World) == kSceneVisible);
    CHECK_FALSE(t.update(true, StereoMode::Screen).has_value());  // the screen shows it
    CHECK(t.update(false, StereoMode::World) == kSceneVisible);   // the cutscene took the camera
    CHECK(t.update(true, StereoMode::World) == kSceneVisible);    // handed back: ends
    CHECK_FALSE(t.update(true, StereoMode::World).has_value());

    t.on_message(3, names);  // fade
    t.on_message(4, names);  // fade done
    CHECK_FALSE(t.update(true, StereoMode::World).has_value());
    t.on_message(99, names);
    CHECK_FALSE(t.update(true, StereoMode::World).has_value());
}

TEST_CASE("head-clip fade waits out the grace period in stereo") {
    FakeClock clock;
    HeadClipDelay delay(clock);
    CHECK(delay.apply(0.2f, false) == 0.2f);  // not in stereo: as the game says
    CHECK(delay.apply(0.2f, true) == 0.2f);   // no clear seen recently

    CHECK(delay.apply(1.0f, true) == 1.0f);
    clock.advance(100ms);
    CHECK(delay.apply(0.2f, true) == kSceneVisible);
    clock.advance(499ms);
    CHECK(delay.apply(0.2f, true) == kSceneVisible);
    clock.advance(1ms);
    CHECK(delay.apply(0.2f, true) == 0.2f);  // 600 ms since the last clear
}

TEST_CASE("stereo AA runs the game's pass by setting and pixel scale") {
    omvr::core::config::Settings s;
    RecordingLog log;
    StereoAa aa(s, log);

    s.rendering.stereo_aa   = omvr::core::config::StereoAA::Auto;
    s.rendering.pixel_scale = 1.0f;
    CHECK(aa.runs_aa_pass(AaMode::Fxaa));
    CHECK(aa.runs_aa_pass(AaMode::SmaaT2x));
    CHECK_FALSE(aa.runs_aa_pass(AaMode::Off));
    s.rendering.pixel_scale = 1.31f;
    CHECK_FALSE(aa.runs_aa_pass(AaMode::Fxaa));
    s.rendering.pixel_scale = 1.3f;
    CHECK(aa.runs_aa_pass(AaMode::Fxaa));
    s.rendering.stereo_aa = omvr::core::config::StereoAA::On;
    s.rendering.pixel_scale = 4.0f;
    CHECK(aa.runs_aa_pass(AaMode::SmaaT1x));
    s.rendering.stereo_aa = omvr::core::config::StereoAA::Off;
    CHECK_FALSE(aa.runs_aa_pass(AaMode::SmaaT1x));

    CHECK(StereoAa::spatial_smaa(true, true));
    CHECK_FALSE(StereoAa::spatial_smaa(true, false));
    CHECK_FALSE(StereoAa::spatial_smaa(false, true));
}

TEST_CASE("stereo AA logs each decision once") {
    omvr::core::config::Settings s;
    RecordingLog log;
    StereoAa aa(s, log);
    aa.log_decision(AaMode::SmaaT2x, true);
    aa.log_decision(AaMode::SmaaT2x, true);
    aa.log_decision(AaMode::Off, false);
    REQUIRE(log.lines.size() == 2);
    CHECK(log.lines[0] == "[stereo] Stereo anti-aliasing for game mode SMAA T2x: runs it as SMAA T1x (setting Auto)");
    CHECK(log.lines[1] == "[stereo] Stereo anti-aliasing for game mode off: nothing to run (setting Auto)");
}

} // namespace
