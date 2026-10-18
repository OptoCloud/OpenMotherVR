// OpenMotherVR-Provenance: clean
// core/input: the per-controller glyphs and controls help rows of spec 22
// §10 (Controls.ControllerGlyphs), the emulated d-pad's glyphs (22 §7) and
// the rows of v2 features (22 §10.3). Expected values are the spec's tables.

#include "core/config/settings.h"
#include "core/input/control_scheme.h"
#include "core/input/glyph_ids.h"
#include "core/input/glyphs.h"
#include "core/input/glyphs_rule.h"
#include "core/input/virtual_pad.h"
#include "fakes/fakes.h"
#include "game/constants.h"

#include <doctest/doctest.h>

#include <algorithm>
#include <cstring>
#include <set>
#include <string>
#include <string_view>

namespace {

using namespace omvr::core::input;
namespace game   = omvr::game;
namespace config = omvr::core::config;
using omvr::tests::RecordingLog;

constexpr auto kSvi    = ControllerSource::SteamVrInput;
constexpr auto kXr     = ControllerSource::OpenXr;
constexpr auto kLegacy = ControllerSource::OpenVrLegacy;

std::string_view glyph(GlyphFamily left, GlyphFamily right, ControllerSource src, int code,
                       bool dpad = false) {
    const char* g = family_glyph(left, right, src, code, dpad);
    return g ? std::string_view{g} : std::string_view{"(none)"};
}

std::string_view same(GlyphFamily f, ControllerSource src, int code, bool dpad = false) {
    return glyph(f, f, src, code, dpad);
}

bool has_row(const HelpRows& rows, std::string_view line) {
    return std::any_of(rows.begin(), rows.end(), [&](const char* l) { return line == l; });
}

TEST_CASE("glyph ids: unique, prefixed, unlike every stock input id") {
    std::set<std::string> seen;
    for (const char* id : kAllGlyphIds) {
        const std::string_view s{id};
        CHECK(s.starts_with("OMVR_"));
        CHECK(seen.insert(std::string{s}).second);
        for (const char* stock : kStockPadNames) CHECK(s != stock);
    }
    CHECK(seen.size() == kAllGlyphIds.size());
}

TEST_CASE("every glyph the rules return has an image: one of the ids or a stock name") {
    const auto known = [](const char* g) {
        const std::string_view s{g};
        return std::any_of(kAllGlyphIds.begin(), kAllGlyphIds.end(),
                           [&](const char* id) { return s == id; }) ||
               std::any_of(kStockPadNames.begin(), kStockPadNames.end(),
                           [&](const char* n) { return s == n; });
    };
    using F            = GlyphFamily;
    const F families[] = {F::Unknown, F::Index, F::Touch, F::Wmr, F::Wand, F::Frame, F::Generic};
    const auto sources = {kSvi, kXr, kLegacy};
    const auto schemes = {ControlScheme::Touchpad, ControlScheme::Thumbstick, ControlScheme::Index,
                          ControlScheme::Frame};
    for (int code = 0; code < static_cast<int>(game::kPadCodeCount); ++code) {
        for (bool dpad : {false, true}) {
            for (F l : families)
                for (F r : families)
                    for (auto src : sources) {
                        const char* g = family_glyph(l, r, src, code, dpad);
                        if (g) CHECK(known(g));
                    }
            for (auto s : schemes) CHECK(known(scheme_glyph(s, code, dpad)));
        }
    }
}

TEST_CASE("families from the runtimes' identification (22 §10.1)") {
    CHECK(glyph_family(kSvi, "knuckles") == GlyphFamily::Index);
    CHECK(glyph_family(kSvi, "oculus_touch") == GlyphFamily::Touch);
    CHECK(glyph_family(kSvi, "holographic_controller") == GlyphFamily::Wmr);
    CHECK(glyph_family(kSvi, "vive_controller") == GlyphFamily::Wand);
    CHECK(glyph_family(kSvi, "frame_controller") == GlyphFamily::Frame);
    CHECK(glyph_family(kSvi, "something_else") == GlyphFamily::Generic);
    CHECK(glyph_family(kSvi, "") == GlyphFamily::Unknown);
    CHECK(glyph_family(kXr, "/interaction_profiles/valve/index_controller") == GlyphFamily::Index);
    CHECK(glyph_family(kXr, "/interaction_profiles/oculus/touch_controller") == GlyphFamily::Touch);
    CHECK(glyph_family(kXr, "/interaction_profiles/microsoft/motion_controller") == GlyphFamily::Wmr);
    CHECK(glyph_family(kXr, "/interaction_profiles/htc/vive_controller") == GlyphFamily::Wand);
    CHECK(glyph_family(kXr, "/interaction_profiles/valve/frame_controller_valve") == GlyphFamily::Frame);
    CHECK(glyph_family(kXr, "/interaction_profiles/khr/simple_controller") == GlyphFamily::Generic);
    CHECK(glyph_family(kXr, "") == GlyphFamily::Unknown);
    // Legacy OpenVR drives every controller as a wand (04 §5.8.3).
    CHECK(glyph_family(kLegacy, "knuckles") == GlyphFamily::Wand);
    CHECK(glyph_family(kLegacy, "") == GlyphFamily::Wand);
}

TEST_CASE("the design per prompt and family (22 §10.2 table)") {
    using F = GlyphFamily;
    SUBCASE("Index") {
        CHECK(same(F::Index, kSvi, game::kPadA) == kGlyphAR);
        CHECK(same(F::Index, kSvi, game::kPadB) == kGlyphRightStick);
        CHECK(same(F::Index, kSvi, game::kPadX) == kGlyphBR);
        CHECK(same(F::Index, kSvi, game::kPadY) == kGlyphBL);
        CHECK(same(F::Index, kSvi, game::kPadBack) == kGlyphAL);
        CHECK(same(F::Index, kSvi, game::kPadStart) == kGlyphTrackpad);
        CHECK(same(F::Index, kXr, game::kPadStart) == kGlyphTrackpad); // the trackpad force, same glyph
        CHECK(same(F::Index, kSvi, game::kPadLB) == kGlyphLeftGrip);
        CHECK(same(F::Index, kSvi, game::kPadRB) == kGlyphRightGrip);
    }
    SUBCASE("Touch and generic: v1's thumbstick set") {
        for (F f : {F::Touch, F::Generic}) {
            CHECK(same(f, kSvi, game::kPadA) == kGlyphA);
            CHECK(same(f, kSvi, game::kPadB) == kGlyphRightStick);
            CHECK(same(f, kSvi, game::kPadX) == kGlyphB);
            CHECK(same(f, kSvi, game::kPadY) == kGlyphY);
            CHECK(same(f, kSvi, game::kPadBack) == kGlyphX);
            CHECK(same(f, kSvi, game::kPadStart) == kGlyphMenu);
        }
    }
    SUBCASE("WMR under SteamVR Input and under OpenXR") {
        CHECK(same(F::Wmr, kSvi, game::kPadA) == kGlyphTouchpadDownR);
        CHECK(same(F::Wmr, kSvi, game::kPadB) == kGlyphRightStick);
        CHECK(same(F::Wmr, kSvi, game::kPadX) == kGlyphTouchpadUpR);
        CHECK(same(F::Wmr, kSvi, game::kPadY) == kGlyphTouchpadUpL);
        CHECK(same(F::Wmr, kSvi, game::kPadBack) == kGlyphTouchpadDownL);
        CHECK(same(F::Wmr, kSvi, game::kPadStart) == kGlyphMenu);
        CHECK(same(F::Wmr, kXr, game::kPadA) == kGlyphTouchpadAnyR);
        CHECK(same(F::Wmr, kXr, game::kPadX) == kGlyphUnbound);
        CHECK(same(F::Wmr, kXr, game::kPadY) == "Y"); // the game's own name
        CHECK(same(F::Wmr, kXr, game::kPadBack) == kGlyphTouchpadAnyL);
        CHECK(same(F::Wmr, kXr, game::kPadStart) == kGlyphMenu);
    }
    SUBCASE("Vive wand, and every controller on legacy OpenVR") {
        for (auto [f, src] :
             {std::pair{F::Wand, kSvi}, std::pair{F::Wand, kXr}, std::pair{F::Index, kLegacy}}) {
            CHECK(same(f, src, game::kPadA) == kGlyphTouchpadDownR);
            CHECK(same(f, src, game::kPadB) == kGlyphTouchpadCentreR);
            CHECK(same(f, src, game::kPadX) == kGlyphTouchpadUpR);
            CHECK(same(f, src, game::kPadY) == "Y");
            CHECK(same(f, src, game::kPadBack) == kGlyphMenuL);
            CHECK(same(f, src, game::kPadStart) == kGlyphMenuR);
            CHECK(same(f, src, game::kPadLB) == kGlyphLeftGrip);
        }
    }
    SUBCASE("Steam Frame") {
        CHECK(same(F::Frame, kSvi, game::kPadA) == kGlyphA);
        CHECK(same(F::Frame, kSvi, game::kPadB) == kGlyphRightStick);
        CHECK(same(F::Frame, kSvi, game::kPadX) == kGlyphB);
        CHECK(same(F::Frame, kSvi, game::kPadY) == kGlyphLeftBumper);
        CHECK(same(F::Frame, kSvi, game::kPadBack) == kGlyphView);
        CHECK(same(F::Frame, kSvi, game::kPadStart) == kGlyphMenu);
        // Its own d-pad: the game's d-pad glyph, with or without the emulation.
        CHECK(same(F::Frame, kSvi, game::kPadDpadUp, true) == "DPAD_UP");
        CHECK(same(F::Frame, kXr, game::kPadDpadLeft, false) == "DPAD_LEFT");
    }
    SUBCASE("every other pad code keeps the game's own name") {
        CHECK(same(F::Index, kSvi, game::kPadRStick) == "RSTICK_BUTTON");
        CHECK(same(F::Touch, kSvi, 0x10) == "LT");
        CHECK(same(F::Touch, kSvi, 0x17) == "TOUCH_ZOOM_OUT");
    }
    SUBCASE("the d-pad only with emulation on (22 §7)") {
        CHECK(same(F::Touch, kSvi, game::kPadDpadUp) == "DPAD_UP");
        CHECK(same(F::Touch, kSvi, game::kPadDpadUp, true) == kGlyphLeftStickUp);
        CHECK(same(F::Index, kSvi, game::kPadDpadDown, true) == kGlyphLeftStickDown);
        CHECK(same(F::Wmr, kXr, game::kPadDpadLeft, true) == kGlyphLeftStickLeft);
        CHECK(same(F::Generic, kSvi, game::kPadDpadRight, true) == kGlyphLeftStickRight);
        CHECK(same(F::Wand, kSvi, game::kPadDpadUp, true) == kGlyphTouchpadUpL);
        CHECK(same(F::Wand, kSvi, game::kPadDpadDown, true) == kGlyphTouchpadDownL);
        CHECK(same(F::Wand, kSvi, game::kPadDpadLeft, true) == kGlyphTouchpadLeftL);
        CHECK(same(F::Wand, kSvi, game::kPadDpadRight, true) == kGlyphTouchpadRightL);
    }
}

TEST_CASE("per hand: left-hand controls take the left controller's design (22 §10.1)") {
    using F = GlyphFamily;
    // Touch in the left hand, Index in the right.
    CHECK(glyph(F::Touch, F::Index, kSvi, game::kPadA) == kGlyphAR);       // right: Index A
    CHECK(glyph(F::Touch, F::Index, kSvi, game::kPadY) == kGlyphY);        // left: Touch Y
    CHECK(glyph(F::Touch, F::Index, kSvi, game::kPadBack) == kGlyphX);     // left: Touch X
    CHECK(glyph(F::Touch, F::Index, kSvi, game::kPadStart) == kGlyphMenu); // left: Touch menu
    // Index in the left hand, Touch in the right.
    CHECK(glyph(F::Index, F::Touch, kSvi, game::kPadA) == kGlyphA);
    CHECK(glyph(F::Index, F::Touch, kSvi, game::kPadBack) == kGlyphAL);
    CHECK(glyph(F::Index, F::Touch, kSvi, game::kPadStart) == kGlyphTrackpad);
    // A hand not identified yet counts as the other hand's family.
    CHECK(glyph(F::Unknown, F::Index, kSvi, game::kPadBack) == kGlyphAL);
    CHECK(glyph(F::Wand, F::Unknown, kSvi, game::kPadA) == kGlyphTouchpadDownR);
    CHECK(family_glyph(F::Unknown, F::Unknown, kSvi, game::kPadA, false) == nullptr);
}

TEST_CASE("v1's sets and the emulated d-pad (04 §4.1, 22 §7)") {
    CHECK(std::string_view{scheme_glyph(ControlScheme::Thumbstick, game::kPadA, false)} == kGlyphA);
    CHECK(std::string_view{scheme_glyph(ControlScheme::Index, game::kPadBack, false)} == kGlyphX);
    CHECK(std::string_view{scheme_glyph(ControlScheme::Touchpad, game::kPadY, false)} == "Y");
    CHECK(std::string_view{scheme_glyph(ControlScheme::Touchpad, game::kPadBack, false)} == kGlyphMenu);
    CHECK(std::string_view{scheme_glyph(ControlScheme::Thumbstick, game::kPadDpadUp, false)} == "DPAD_UP");
    CHECK(std::string_view{scheme_glyph(ControlScheme::Thumbstick, game::kPadDpadUp, true)} ==
          kGlyphLeftStickUp);
    CHECK(std::string_view{scheme_glyph(ControlScheme::Touchpad, game::kPadDpadLeft, true)} ==
          kGlyphTouchpadLeftL);
    CHECK(std::string_view{scheme_glyph(ControlScheme::Frame, game::kPadDpadLeft, true)} == "DPAD_LEFT");
}

TEST_CASE("ControllerGlyphs off: v1's glyphs and rows whatever the controllers") {
    Glyphs glyphs;
    config::ControlsSettings controls;
    RecordingLog log;
    ControlSchemes schemes(glyphs, controls, log);
    schemes.set(ControlScheme::Index);
    schemes.identify(0, kSvi, "knuckles");
    schemes.identify(1, kSvi, "knuckles");
    CHECK(std::string_view{glyphs.name(game::kPadA)} == kGlyphA);
    CHECK(std::string_view{glyphs.name(game::kPadBack)} == kGlyphX);
    const HelpRows rows = schemes.control_help();
    CHECK(rows.size() == 16u);
    CHECK(std::string_view{rows.back()} == "Pause Menu\tLeft Trackpad Press");
    CHECK(std::string_view{rows[7]} == "Interact\tA");
    CHECK_FALSE(glyphs.take_redraw()); // v1 never asks for a redraw
    CHECK(log.lines.empty());
}

TEST_CASE("ControllerGlyphs on: glyphs and rows follow the families, and a change redraws") {
    Glyphs glyphs;
    config::ControlsSettings controls;
    controls.controller_glyphs = true;
    RecordingLog log;
    ControlSchemes schemes(glyphs, controls, log);
    schemes.set(ControlScheme::Index); // v1's choice at the first identification
    schemes.identify(0, kSvi, "knuckles");
    schemes.identify(1, kSvi, "knuckles");
    CHECK(std::string_view{glyphs.name(game::kPadA)} == kGlyphAR);
    CHECK(std::string_view{glyphs.name(game::kPadBack)} == kGlyphAL);
    CHECK(glyphs.take_redraw());
    CHECK(log.contains("right Index"));

    HelpRows rows = schemes.control_help();
    REQUIRE(rows.size() == 16u);
    CHECK(std::string_view{rows[7]} == "Interact\tRight A");
    CHECK(std::string_view{rows[9]} == "Reload / Activate\tRight B");
    CHECK(std::string_view{rows[10]} == "Flashlight\tLeft B");
    CHECK(std::string_view{rows[11]} == "Flashlight [Press Interact near Head]\tRight A");
    CHECK(std::string_view{rows[14]} == "Map Menu\tLeft A");
    CHECK(std::string_view{rows[15]} == "Pause Menu\tLeft Trackpad Press");

    // Touch controllers picked up during play: new glyphs, a redraw, a log line.
    schemes.identify(0, kSvi, "oculus_touch");
    schemes.identify(1, kSvi, "oculus_touch");
    CHECK(std::string_view{glyphs.name(game::kPadA)} == kGlyphA);
    CHECK(std::string_view{glyphs.name(game::kPadBack)} == kGlyphX);
    CHECK(glyphs.take_redraw());
    CHECK_FALSE(glyphs.take_redraw()); // taken once
    CHECK(log.contains("left Touch, right Touch"));
    rows = schemes.control_help();
    CHECK(std::string_view{rows[7]} == "Interact\tA");

    // The same device again changes nothing.
    log.lines.clear();
    schemes.identify(1, kSvi, "oculus_touch");
    CHECK_FALSE(glyphs.take_redraw());
    CHECK(log.lines.empty());
}

TEST_CASE("ControllerGlyphs: help rows of WMR and the Frame (22 §10.3)") {
    Glyphs glyphs;
    config::ControlsSettings controls;
    controls.controller_glyphs = true;
    RecordingLog log;
    ControlSchemes schemes(glyphs, controls, log);
    schemes.identify(1, kSvi, "holographic_controller");
    HelpRows rows = schemes.control_help();
    REQUIRE(rows.size() == 16u);
    CHECK(std::string_view{rows[7]} == "Interact\tRight Touchpad Down");
    CHECK(std::string_view{rows[9]} == "Reload / Activate\tRight Touchpad Up");
    CHECK(std::string_view{rows[10]} == "Flashlight\tLeft Touchpad Up");
    CHECK(std::string_view{rows[11]} == "Flashlight [Press Interact near Head]\tRight Touchpad Down");
    CHECK(std::string_view{rows[14]} == "Map Menu\tLeft Touchpad Down");

    schemes.identify(1, kXr, "/interaction_profiles/microsoft/motion_controller");
    rows = schemes.control_help();
    REQUIRE(rows.size() == 14u); // rows 10 and 11 left out
    CHECK(std::string_view{rows[7]} == "Interact\tRight Touchpad Click");
    CHECK(std::string_view{rows[8]} == "Radial Menu\tRight Thumbstick Click");
    CHECK(std::string_view{rows[9]} == "Flashlight [Press Interact near Head]\tRight Touchpad Click");
    CHECK(std::string_view{rows[12]} == "Map Menu\tLeft Touchpad Click");

    schemes.identify(1, kXr, "/interaction_profiles/valve/frame_controller_valve");
    rows = schemes.control_help();
    REQUIRE(rows.size() == 17u);
    CHECK(std::string_view{rows[10]} == "Flashlight\tLeft Bumper");
    CHECK(std::string_view{rows[14]} == "Map Menu\tView Button");
    CHECK(std::string_view{rows[16]} == "Menu Navigation\tLeft D-Pad");
    CHECK(std::string_view{glyphs.name(game::kPadY)} == kGlyphLeftBumper);

    schemes.identify(1, kSvi, "vive_controller");
    rows = schemes.control_help();
    CHECK(rows.size() == 13u);
    CHECK(std::string_view{rows.back()} == "Pause Menu\tR Menu");
}

TEST_CASE("ControllerGlyphs applies at the next identification") {
    Glyphs glyphs;
    config::ControlsSettings controls;
    RecordingLog log;
    ControlSchemes schemes(glyphs, controls, log);
    schemes.set(ControlScheme::Index);
    schemes.identify(1, kSvi, "knuckles");
    controls.controller_glyphs = true;
    schemes.refresh();
    CHECK(std::string_view{glyphs.name(game::kPadA)} == kGlyphA); // not yet
    schemes.identify(0, kSvi, "knuckles");
    CHECK(std::string_view{glyphs.name(game::kPadA)} == kGlyphAR);
    controls.controller_glyphs = false;
    schemes.identify(0, kSvi, "oculus_touch");
    CHECK(std::string_view{glyphs.name(game::kPadA)} == kGlyphA); // v1 again
    CHECK(std::string_view{schemes.control_help()[7]} == "Interact\tA");
}

TEST_CASE("d-pad emulation: glyphs at once, with ControllerGlyphs off too") {
    Glyphs glyphs;
    config::ControlsSettings controls;
    RecordingLog log;
    ControlSchemes schemes(glyphs, controls, log);
    schemes.set(ControlScheme::Thumbstick);
    CHECK(std::string_view{glyphs.name(game::kPadDpadUp)} == "DPAD_UP");
    controls.dpad_emulation = true;
    schemes.refresh();
    CHECK(std::string_view{glyphs.name(game::kPadDpadUp)} == kGlyphLeftStickUp);
    CHECK(std::string_view{glyphs.name(game::kPadDpadRight)} == kGlyphLeftStickRight);
    CHECK(glyphs.take_redraw());
    controls.dpad_emulation = false;
    schemes.refresh();
    CHECK(std::string_view{glyphs.name(game::kPadDpadUp)} == "DPAD_UP");
    CHECK(glyphs.take_redraw()); // back to the stock glyph: redrawn too
}

TEST_CASE("rows of v2 features, appended while their keys are on (22 §10.3)") {
    Glyphs glyphs;
    config::ControlsSettings controls;
    RecordingLog log;
    ControlSchemes schemes(glyphs, controls, log);
    schemes.set(ControlScheme::Thumbstick);
    CHECK(schemes.control_help().size() == 16u);

    controls.dpad_emulation      = true;
    controls.recenter_gesture    = true;
    controls.hud_toggle_gesture  = true;
    controls.holster_gestures    = true;
    controls.hiding_lean         = true;
    controls.hold_breath_gesture = true;
    HelpRows rows                = schemes.control_help();
    REQUIRE(rows.size() == 16u + 9u);
    CHECK(std::string_view{rows[16]} == "D-Pad\tHold Left Thumbstick Click + Left Thumbstick");
    CHECK(std::string_view{rows[17]} == "Recenter [Both Hands near Head]\tHold Both Grips");
    CHECK(std::string_view{rows[18]} == "Hide/Show HUD [Left Hand near Head]\tX");
    CHECK(std::string_view{rows[19]} == "Next Weapon [Grip over Right Shoulder]\tRight Grip");
    CHECK(std::string_view{rows[20]} == "Previous Weapon [Grip over Left Shoulder]\tLeft Grip");
    CHECK(std::string_view{rows[21]} == "Flashlight [Grip at Left Hip]\tLeft Grip");
    CHECK(std::string_view{rows[22]} == "Motion Tracker [Grip at Chest]\tLeft or Right Grip");
    CHECK(std::string_view{rows[23]} == "Lean [While Hiding]\tLean Your Head");
    CHECK(std::string_view{rows[24]} == "Hold Breath [While Hiding]\tHand over Mouth");

    // The wand style.
    schemes.set(ControlScheme::Touchpad);
    rows = schemes.control_help();
    REQUIRE(rows.size() == 13u + 9u);
    CHECK(std::string_view{rows[13]} == "D-Pad\tL Touchpad Edge Click");
    CHECK(std::string_view{rows[15]} == "Hide/Show HUD [Left Hand near Head]\tL Menu");
    CHECK(std::string_view{rows[16]} == "Next Weapon [Grip over Right Shoulder]\tR Grip");
    CHECK(std::string_view{rows[19]} == "Motion Tracker [Grip at Chest]\tL or R Grip");

    // With ControllerGlyphs, the HUD row names the family's Map control.
    controls.controller_glyphs = true;
    schemes.identify(1, kSvi, "knuckles");
    CHECK(has_row(schemes.control_help(), "Hide/Show HUD [Left Hand near Head]\tLeft A"));
    // Acceptance 22 §18 item 12: the list ends with the recenter row when it
    // alone is on.
    controls.dpad_emulation      = false;
    controls.hud_toggle_gesture  = false;
    controls.holster_gestures    = false;
    controls.hiding_lean         = false;
    controls.hold_breath_gesture = false;
    CHECK(std::string_view{schemes.control_help().back()} ==
          "Recenter [Both Hands near Head]\tHold Both Grips");
}

TEST_CASE("a glyph change redraws the prompts through the pad's activity hook") {
    Glyphs glyphs;
    RecordingLog log;
    VirtualPad pad(glyphs, log);
    CHECK(pad.on_activity(false, PlayerDevices::Both).devices == DeviceSwitch::None);
    glyphs.request_redraw();
    // Not while the input manager is missing: kept for later.
    CHECK(pad.on_activity(false, PlayerDevices::NoManager).devices == DeviceSwitch::None);
    CHECK(pad.on_activity(false, PlayerDevices::Both).devices == DeviceSwitch::PulseKeyboard);
    CHECK(pad.on_activity(false, PlayerDevices::Both).devices == DeviceSwitch::RestorePad);
    CHECK(pad.on_activity(false, PlayerDevices::Both).devices == DeviceSwitch::None);
}

} // namespace
