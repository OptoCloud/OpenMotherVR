// core/input: touchpad regions, the controller schemes, the tier writes and
// grip toggles of the control mapper, the action-map remap on fake maps,
// the pad merge rules, the glyph table and the interaction state.

#include "core/config/settings.h"
#include "core/input/action_tiers.h"
#include "core/input/control_mapper.h"
#include "core/input/control_scheme.h"
#include "core/input/controller_mapping.h"
#include "core/input/glyph_ids.h"
#include "core/input/glyphs.h"
#include "core/input/interaction_state.h"
#include "core/input/remap.h"
#include "core/input/touchpad.h"
#include "core/input/virtual_pad.h"
#include "fakes/fakes.h"
#include "game/constants.h"

#include <doctest/doctest.h>

#include <array>
#include <cmath>
#include <cstdint>
#include <limits>
#include <string_view>

namespace {

using namespace omvr::core::input;
namespace game = omvr::game;
using omvr::tests::RecordingLog;

// The left stick walks the player (no menu, no interaction).
class FixedStick final : public ports::StickContext {
public:
    bool stick_free() const noexcept override { return false; }
};

TEST_CASE("touchpad regions: down interacts, up reloads, the centre opens the radial") {
    TouchpadRegions r = decode_touchpad({0.0f, -0.8f}, true);
    CHECK(r.interact);
    CHECK_FALSE(r.reload);
    CHECK_FALSE(r.radial);

    r = decode_touchpad({0.0f, 0.8f}, true);
    CHECK(r.reload);

    r = decode_touchpad({0.1f, -0.1f}, true);
    CHECK(r.centre);
    CHECK(r.radial);
    CHECK_FALSE(r.interact);

    r = decode_touchpad({0.8f, 0.1f}, true); // an edge to the side: nothing
    CHECK(r.horizontal);
    CHECK_FALSE(r.interact);
    CHECK_FALSE(r.reload);
    CHECK_FALSE(r.radial);

    r = decode_touchpad({0.0f, -0.8f}, false);
    CHECK_FALSE(r.interact);
}

TEST_CASE("the touchpad scheme: move and turn only while touched") {
    HandInput left{.stick = {0.5f, 0.5f}, .stick_touched = false, .stick_click = true, .menu = true};
    HandInput right{
        .stick = {0.0f, -0.9f}, .stick_touched = true, .stick_click = true, .trigger = 0.7f, .grip = true};
    const Controls c = touchpad_controls(left, right);
    CHECK(c.touchpad_scheme);
    CHECK(c.move.x == 0.0f); // left pad not touched
    CHECK(c.turn.y == -0.9f);
    CHECK(c.turn_engaged);
    CHECK(c.interact);
    CHECK(c.sprint);
    CHECK(c.map);
    CHECK_FALSE(c.pause);
    CHECK(c.trigger_r == 0.7f);
    CHECK(c.grip_r);
}

TEST_CASE("the thumbstick scheme: face buttons") {
    HandInput left{.stick = {0.2f, 0.3f}, .primary = true, .secondary = true};
    HandInput right{
        .stick = {0.4f, 0.0f}, .stick_click = true, .primary = true, .secondary = false, .menu = true};
    const Controls c = thumbstick_controls(left, right);
    CHECK_FALSE(c.touchpad_scheme);
    CHECK(c.move.y == 0.3f);
    CHECK(c.turn.x == 0.4f);
    CHECK(c.radial);
    CHECK(c.interact);
    CHECK_FALSE(c.reload);
    CHECK(c.map);
    CHECK(c.flashlight);
    CHECK(c.pause);

    CHECK(squeeze_pressed(0.51f));
    CHECK_FALSE(squeeze_pressed(0.5f));
    CHECK(near_head({0.0f, 0.0f, 0.1f}, {0.0f, 0.0f, 0.0f}));
    CHECK_FALSE(near_head({0.0f, 0.0f, 0.3f}, {0.0f, 0.0f, 0.0f}));
}

TEST_CASE("action tiers: bases, lookups and the left-stick prompt") {
    ActionTiers tiers = make_tiers();
    CHECK(tiers[0].base == 0x100);
    CHECK(tiers[1].base == 0x200);
    CHECK(tiers[2].base == 0x400);
    CHECK(&tier_for(tiers, 0x250) == &tiers[1]);
    CHECK(&tier_for(tiers, 5) == &tiers[0]);
    CHECK(&tier_for(tiers, 0x500) == &tiers[2]);
    CHECK(stock_code(tiers, 0x100) == game::kNoPadControl);
    CHECK(stock_code(tiers, 0x100 + 200) == game::kNoPadControl); // past the tier

    tiers[0].num_digital = 0x20;
    tiers[0].num_analog  = 4;
    CHECK(analog_virtual_code(tiers, 0, 3) == 0x100 + 0x20 + 3);
    CHECK(analog_virtual_code(tiers, 0, 4) == game::kNoPadControl);
    CHECK(analog_virtual_code(tiers, 3, 0) == game::kNoPadControl);

    CHECK(move_on_left_stick(tiers)); // nothing recorded yet
    tiers[0].stock_code[0x20 + game::player_action::kMoveX] = game::kPadLStickX;
    CHECK(move_on_left_stick(tiers));
    tiers[0].stock_code[0x20 + game::player_action::kMoveX] = 0x14;
    CHECK_FALSE(move_on_left_stick(tiers));

    tiers[0].analog[1] = 0.25f;
    CHECK(vr_analog(tiers[0], 0x100 + 0x20 + 1) == 0.25f);
    CHECK(vr_analog(tiers[0], 0x100 + 1) == 0.0f); // a digital code
}

// Action-map structs of the game's shape (04 §2.4).
struct FakeBinding {
    std::int16_t code;
    std::uint8_t device;
    std::uint8_t flags;
};
struct FakeAction {
    FakeBinding bindings[game::kMaxBindings];
    int count;
};
struct FakeComposite {
    std::uint32_t codes[game::kCompositeCodes];
    int count;
    bool keep;
};
struct FakeMap {
    FakeAction digital[game::kMaxDigitalActions];
    FakeAction analog[game::kMaxAnalogActions];
    FakeComposite composite[game::kMaxComposites];
    int num_digital;
    int num_analog;
    int composite_count;
};
static_assert(ActionMap<FakeMap>);

constexpr std::uint8_t kKeyboard = 1;

TEST_CASE("remap binds every action to its virtual code") {
    FakeMap map{};
    map.num_digital = 3;
    map.num_analog  = 1;
    // Action 0: a key and two pad bindings; the first carries a composite flag.
    map.digital[0].bindings[0] = {0x1C, kKeyboard, 0};
    map.digital[0].bindings[1] = {game::kPadA, game::kBindingDevicePad, game::kBindingCompositeRef | 1};
    map.digital[0].bindings[2] = {game::kPadB, game::kBindingDevicePad, 0};
    map.digital[0].count       = 3;
    // Action 1: no pad binding.
    map.digital[1].bindings[0] = {0x1D, kKeyboard, 0};
    map.digital[1].count       = 1;
    // Action 2: six records, none for the pad.
    for (auto& b : map.digital[2].bindings) b = {0x20, kKeyboard, 0};
    map.digital[2].count = game::kMaxBindings;
    // Analog 0: the left stick.
    map.analog[0].bindings[0] = {game::kPadLStickX, game::kBindingDevicePad, game::kBindingCompositeRef};
    map.analog[0].count       = 1;
    map.composite_count       = 2;
    map.composite[0]          = {{1, 2, 3}, 2, false};
    map.composite[1]          = {{1, 2, 3}, 2, true};

    ActionTiers tiers = make_tiers();
    ActionTier& t     = tiers[0];
    remap(t, map);

    CHECK(t.num_digital == 3);
    CHECK(t.num_analog == 1);
    CHECK(map.digital[0].bindings[0].code == 0x1C);
    CHECK(map.digital[0].bindings[1].code == 0x100);
    CHECK(map.digital[0].bindings[1].flags == 1); // composite flag cleared
    CHECK(map.digital[0].bindings[2].code == game::kNoPadControl);
    CHECK(t.stock_code[0] == game::kPadA);

    CHECK(map.digital[1].count == 2);
    CHECK(map.digital[1].bindings[1].code == 0x101);
    CHECK(map.digital[1].bindings[1].device == game::kBindingDevicePad);
    CHECK(map.digital[1].bindings[1].flags == kAppendedFlagsDigital);
    CHECK(t.stock_code[1] == game::kNoPadControl);

    CHECK(map.digital[2].count == game::kMaxBindings); // full: nothing appended
    CHECK(t.stock_code[2] == game::kNoPadControl);

    CHECK(map.analog[0].bindings[0].code == 0x103);
    CHECK(map.analog[0].bindings[0].flags == game::kBindingCompositeRef); // analog keeps its flags
    CHECK(t.stock_code[3] == game::kPadLStickX);
    CHECK(t.stock_code[4] == game::kNoPadControl);

    CHECK(map.composite[0].codes[0] == game::kNoCompositeCode);
    CHECK(map.composite[0].codes[1] == game::kNoCompositeCode);
    CHECK(map.composite[0].codes[2] == 3u); // past its count
    CHECK(map.composite[1].codes[0] == 1u); // kept

    // Idempotent.
    const FakeMap before = map;
    remap(t, map);
    CHECK(map.digital[1].count == before.digital[1].count);
    CHECK(map.digital[0].bindings[1].code == 0x100);
    CHECK(t.stock_code[0] == game::kPadA);
}

TEST_CASE("the pad merge rules") {
    Glyphs glyphs;
    RecordingLog log;
    VirtualPad pad(glyphs, log);
    ActionTier& t   = pad.tiers()[0];
    t.num_digital   = 2;
    t.num_analog    = 1;
    t.digital[1]    = 1;
    t.analog[0]     = 0.6f;
    t.stock_code[0] = game::kPadX;

    CHECK(pad.merge_down(0x101, false));
    CHECK_FALSE(pad.merge_down(0x100, false));
    CHECK(pad.merge_down(0x100, true));
    CHECK_FALSE(pad.merge_down(0x105, false)); // past the actions in use

    CHECK(pad.merge_value(0x102, 0.4f) == 0.6f);
    CHECK(pad.merge_value(0x102, 0.8f) == 0.8f);
    CHECK(pad.merge_axis(0x102, -0.7f) == -0.7f);
    CHECK(pad.merge_axis(0x102, 0.1f) == 0.6f);

    CHECK(VirtualPad::asks_for_remap(game::kPadA));
    CHECK_FALSE(VirtualPad::asks_for_remap(0x100));
    CHECK_FALSE(VirtualPad::asks_for_remap(game::kNoPadControl));
    CHECK(pad.stock_code(0x100) == game::kPadX);

    CHECK(std::string_view{pad.vr_glyph(-1)} == kGlyphUnbound);
    CHECK(std::string_view{pad.vr_glyph(99)} == kGlyphUnbound);
    CHECK(std::string_view{pad.vr_glyph(game::kPadA)} == "A");
}

TEST_CASE("pad activity switches the prompts between VR and stock glyphs") {
    Glyphs glyphs;
    RecordingLog log;
    VirtualPad pad(glyphs, log);
    CHECK(pad.shows_vr_glyphs());

    pad.note_vr_activity();
    ActivityResult r = pad.on_activity(false, PlayerDevices::Both);
    CHECK(r.any);
    CHECK(r.devices == DeviceSwitch::None); // already VR glyphs

    r = pad.on_activity(true, PlayerDevices::Both); // the real pad
    CHECK(r.any);
    CHECK(r.devices == DeviceSwitch::PulseKeyboard);
    CHECK_FALSE(pad.shows_vr_glyphs());

    r = pad.on_activity(false, PlayerDevices::Both);
    CHECK_FALSE(r.any);
    CHECK(r.devices == DeviceSwitch::RestorePad);

    pad.note_vr_activity();
    r = pad.on_activity(false, PlayerDevices::NoManager); // nothing touched without a manager
    CHECK(r.any);
    CHECK(r.devices == DeviceSwitch::None);
    CHECK_FALSE(pad.shows_vr_glyphs());

    pad.use_stock_glyphs_only();
    pad.use_stock_glyphs_only();
    CHECK(log.lines.size() == 1);
    pad.note_vr_activity();
    r = pad.on_activity(false, PlayerDevices::Both);
    CHECK(r.devices == DeviceSwitch::None); // VR glyphs not allowed
    CHECK_FALSE(pad.shows_vr_glyphs());
}

TEST_CASE("the control mapper writes the player and menu tiers") {
    Glyphs glyphs;
    RecordingLog log;
    VirtualPad pad(glyphs, log);
    ControlSchemes schemes(glyphs);
    omvr::core::config::InputSettings settings;
    omvr::core::config::ControlsSettings controls;
    FixedStick stick_context;
    omvr::tests::FakeClock clock;
    ControlMapper mapper({pad, schemes, settings, controls, stick_context, clock, log});

    Controls c;
    c.trigger_r = 0.9f;
    c.trigger_l = 0.2f;
    c.interact  = true;
    c.move      = {0.3f, 0.4f};
    c.turn      = {0.9f, 0.0f};
    mapper.write(c, false);

    const ActionTier& player = pad.tiers()[game::kPlayerMap];
    const ActionTier& menu   = pad.tiers()[game::kMenuMap];
    namespace pa             = game::player_action;
    namespace ma             = game::menu_action;
    CHECK(player.digital[pa::kFire] == 1);
    CHECK(player.digital[pa::kFire2] == 1);
    CHECK(player.digital[pa::kCrouch] == 0);
    CHECK(player.digital[pa::kInteract] == 1);
    CHECK(player.digital[pa::kFlashlight] == 0);
    CHECK(player.analog[pa::kMoveX] == doctest::Approx(0.3f));
    CHECK(player.analog[pa::kTurn] == doctest::Approx(0.9f));
    CHECK(menu.digital[ma::kConfirm] == 1);
    CHECK(menu.analog[ma::kTriggerR] == doctest::Approx(0.9f));
    // A trigger past kActivityThreshold counts as VR activity.
    CHECK(pad.on_activity(false, PlayerDevices::NoManager).any);

    // Interact near the head is the flashlight.
    mapper.write(c, true);
    CHECK(player.digital[pa::kFlashlight] == 1);
}

TEST_CASE("the control mapper: turn, weapons, move and clamps") {
    Glyphs glyphs;
    RecordingLog log;
    VirtualPad pad(glyphs, log);
    ControlSchemes schemes(glyphs);
    omvr::core::config::InputSettings settings;
    omvr::core::config::ControlsSettings controls;
    FixedStick stick_context;
    omvr::tests::FakeClock clock;
    ControlMapper mapper({pad, schemes, settings, controls, stick_context, clock, log});
    const ActionTier& player = pad.tiers()[game::kPlayerMap];
    namespace pa             = game::player_action;

    Controls pad_turn;
    pad_turn.touchpad_scheme = true;
    pad_turn.turn            = {0.9f, 0.0f};
    pad_turn.turn_engaged    = false;
    mapper.write(pad_turn, false);
    CHECK(player.analog[pa::kTurn] == 0.0f); // snap rotation: a touchpad turn needs a click
    pad_turn.turn_engaged = true;
    mapper.write(pad_turn, false);
    CHECK(player.analog[pa::kTurn] == doctest::Approx(0.9f * kTouchpadTurnScale));

    Controls stick;
    stick.turn = {0.0f, 0.8f};
    mapper.write(stick, false);
    CHECK(player.digital[pa::kNextWeapon] == 1);
    CHECK(player.digital[pa::kPrevWeapon] == 0);
    stick.turn = {0.0f, -0.8f};
    mapper.write(stick, false);
    CHECK(player.digital[pa::kPrevWeapon] == 1);

    Controls diagonal;
    diagonal.touchpad_scheme = true;
    diagonal.move            = {1.0f, 1.0f};
    mapper.write(diagonal, false);
    CHECK(player.analog[pa::kMoveX] == doctest::Approx(std::sqrt(0.5f)));
    CHECK(player.analog[pa::kMoveY] == doctest::Approx(std::sqrt(0.5f)));

    Controls wild;
    wild.move      = {std::numeric_limits<float>::quiet_NaN(), 0.0f};
    wild.trigger_l = 1.5f;
    mapper.write(wild, false);
    CHECK(player.analog[pa::kMoveX] == 0.0f);
    CHECK(pad.tiers()[game::kMenuMap].analog[game::menu_action::kTriggerL] == 1.0f);
}

TEST_CASE("the control mapper: grips hold, or toggle") {
    Glyphs glyphs;
    RecordingLog log;
    VirtualPad pad(glyphs, log);
    ControlSchemes schemes(glyphs);
    omvr::core::config::InputSettings settings;
    omvr::core::config::ControlsSettings controls;
    FixedStick stick_context;
    omvr::tests::FakeClock clock;
    ControlMapper mapper({pad, schemes, settings, controls, stick_context, clock, log});
    const ActionTier& player = pad.tiers()[game::kPlayerMap];
    namespace pa             = game::player_action;

    Controls c;
    c.grip_r = true;
    c.grip_l = true;
    mapper.write(c, false);
    CHECK(player.digital[pa::kAim] == 1);
    CHECK(player.digital[pa::kTracker] == 1);
    c.radial = true; // the left grip with the radial menu open is not the tracker
    mapper.write(c, false);
    CHECK(player.digital[pa::kTracker] == 0);

    settings.toggle_grips = true;
    Controls press;
    press.grip_r = true;
    mapper.write(press, false); // held from before: no edge
    CHECK(player.digital[pa::kAim] == 0);
    Controls release;
    mapper.write(release, false);
    mapper.write(press, false); // press: on
    CHECK(player.digital[pa::kAim] == 1);
    mapper.write(release, false);
    CHECK(player.digital[pa::kAim] == 1); // stays on
    mapper.write(press, false);           // press again: off
    CHECK(player.digital[pa::kAim] == 0);
}

TEST_CASE("glyph table and the schemes' glyphs and help") {
    Glyphs glyphs;
    CHECK(std::string_view{glyphs.name(game::kPadA)} == "A");
    CHECK(std::string_view{glyphs.name(game::kPadStart)} == "START");
    CHECK(glyphs.size() == game::kPadCodeCount);
    glyphs.set_glyph(99, "X");    // ignored
    glyphs.set_glyph(0, nullptr); // ignored
    CHECK(std::string_view{glyphs.name(0)} == "A");

    ControlSchemes schemes(glyphs);
    CHECK(schemes.control_help().size() == 13u); // touchpad until a controller is seen
    schemes.set(ControlScheme::Thumbstick);
    CHECK(std::string_view{glyphs.name(game::kPadA)} == kGlyphA);
    CHECK(std::string_view{glyphs.name(game::kPadB)} == kGlyphRightStick);
    CHECK(std::string_view{glyphs.name(game::kPadStart)} == kGlyphMenu);
    CHECK(std::string_view{glyphs.name(game::kPadLB)} == kGlyphLeftGrip);
    CHECK(std::string_view{glyphs.name(game::kPadRB)} == kGlyphRightGrip);
    CHECK(schemes.control_help().size() == 16u);
    CHECK(std::string_view{schemes.control_help().front()} == "Move\tLeft Thumbstick");

    schemes.set(ControlScheme::Touchpad);
    CHECK(std::string_view{glyphs.name(game::kPadA)} == kGlyphTouchpadDown);
    CHECK(std::string_view{glyphs.name(game::kPadY)} == "Y");
    CHECK(std::string_view{schemes.control_help().back()} == "Pause Menu\tR Menu");

    schemes.set(ControlScheme::Index);
    CHECK(std::string_view{schemes.control_help().back()} == "Pause Menu\tLeft Trackpad Press");

    // Steam Frame (04 §5.7): the thumbstick glyphs, its own buttons in the help.
    schemes.set(ControlScheme::Frame);
    CHECK(std::string_view{glyphs.name(game::kPadA)} == kGlyphA);
    const auto frame = schemes.control_help();
    CHECK(frame.size() == 17u);
    CHECK(std::string_view{frame[10]} == "Flashlight\tLeft Bumper");
    CHECK(std::string_view{frame[14]} == "Map Menu\tView Button");
    CHECK(std::string_view{frame[15]} == "Pause Menu\tMenu Button");
    CHECK(std::string_view{frame[16]} == "Menu Navigation\tLeft D-Pad");
}

TEST_CASE("interaction state from the scripts' event strings") {
    RecordingLog log;
    InteractionState s(log);
    CHECK_FALSE(s.on_event("Chatter"));
    CHECK_FALSE(s.on_event("on_frame"));
    CHECK_FALSE(s.on_event(""));

    CHECK(s.on_event("start_interaction"));
    CHECK(s.in_interaction());
    CHECK(s.interaction() == Interaction::Generic);

    CHECK(s.on_event("keypad_interaction_start"));
    CHECK(s.interaction() == Interaction::Keypad);
    CHECK(s.on_event("keypad_interaction_finish"));
    CHECK(s.interaction() == Interaction::Generic);
    CHECK(s.on_event("hacking_interaction_finish")); // another kind: unchanged
    CHECK(s.interaction() == Interaction::Generic);

    CHECK(s.on_event("stop_interaction"));
    CHECK_FALSE(s.in_interaction());
    CHECK(s.interaction() == Interaction::None);

    CHECK(s.on_event("hiding_visibility_start"));
    CHECK(s.hiding());
    CHECK(s.on_event("ladder_interaction_start"));
    CHECK(s.on_event("hiding_visibility_finish"));
    CHECK_FALSE(s.hiding());
    CHECK_FALSE(s.in_interaction()); // also ends any interaction

    CHECK_FALSE(s.on_event("something_else"));
}

TEST_CASE("interaction state logs a string the first time and on a change") {
    RecordingLog log;
    InteractionState s(log);
    s.log_event("start_interaction", true);
    s.log_event("start_interaction", false);
    s.log_event("start_interaction", true);
    REQUIRE(log.lines.size() == 2);
    CHECK(log.lines[0] ==
          "[events] \"start_interaction\" (new): interaction false kind 0 hiding false breathing false");
    CHECK(log.lines[1] == "[events] \"start_interaction\": interaction false kind 0 hiding false breathing false");
}

} // namespace
