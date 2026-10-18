// OpenMotherVR-Provenance: clean
// core/input: the d-pad of spec 22 §7: the stick direction with its
// hysteresis, the emulation's context and modifier, the pad driving the
// actions whose stock control is a d-pad direction, and the control mapper
// handing a controller's own d-pad and the emulation to the pad.

#include "core/config/settings.h"
#include "core/input/control_mapper.h"
#include "core/input/control_scheme.h"
#include "core/input/controller_mapping.h"
#include "core/input/dpad_emulation.h"
#include "core/input/glyphs.h"
#include "core/input/virtual_pad.h"
#include "core/input_rig.h"
#include "fakes/fakes.h"
#include "game/constants.h"

#include <doctest/doctest.h>

#include <chrono>
#include <cstddef>

namespace {

using namespace omvr::core::input;
namespace game = omvr::game;
using namespace std::chrono_literals;

constexpr Dpad kUp{.up = true};
constexpr Dpad kDown{.down = true};
constexpr Dpad kLeft{.left = true};
constexpr Dpad kRight{.right = true};
constexpr Dpad kNone{};

// A pad whose maps put d-pad directions where the game's defaults do (22
// §2.2): map 0 prev / next main weapon on left / right, map 1 up / down /
// left / right on the d-pad; and one map 2 action on DPAD_UP.
struct DpadPad : omvr::tests::MapperRig {
    static constexpr int kPrevMainWeapon = 0x1A;
    static constexpr int kNextMainWeapon = 0x1B;
    static constexpr int kMenuUp         = 2;
    static constexpr int kMenuDown       = 3;
    static constexpr int kMenuLeft       = 4;
    static constexpr int kMenuRight      = 5;

    DpadPad() {
        auto& tiers = pad.tiers();
        auto stock  = [](ActionTier& t, int k, int code) {
            t.stock_code[static_cast<std::size_t>(k)] = static_cast<std::int16_t>(code);
        };
        tiers[0].num_digital = game::kMaxDigitalActions;
        tiers[0].num_analog  = 4;
        stock(tiers[0], kPrevMainWeapon, game::kPadDpadLeft);
        stock(tiers[0], kNextMainWeapon, game::kPadDpadRight);
        stock(tiers[0], 0, game::kPadA);
        tiers[1].num_digital = 19;
        tiers[1].num_analog  = 6;
        stock(tiers[1], kMenuUp, game::kPadDpadUp);
        stock(tiers[1], kMenuDown, game::kPadDpadDown);
        stock(tiers[1], kMenuLeft, game::kPadDpadLeft);
        stock(tiers[1], kMenuRight, game::kPadDpadRight);
        // An analog entry recorded with a d-pad code is no digital action.
        stock(tiers[1], tiers[1].num_digital + 2, game::kPadDpadUp);
        tiers[2].num_digital = 32;
        stock(tiers[2], 0, game::kPadDpadUp);
    }

    bool down(int map, int k) const {
        return pad.merge_down(game::kTierBase[static_cast<std::size_t>(map)] + k, false);
    }
};

} // namespace

TEST_CASE("d-pad: the stick's direction, one at a time, with hysteresis (22 §7)") {
    CHECK(stick_dpad({0.0f, 0.7f}, kNone) == kUp);
    CHECK(stick_dpad({0.0f, -0.9f}, kNone) == kDown);
    CHECK(stick_dpad({-0.8f, 0.0f}, kNone) == kLeft);
    CHECK(stick_dpad({0.7f, 0.65f}, kNone) == kRight); // the larger axis
    CHECK(stick_dpad({0.0f, 0.6f}, kNone) == kNone);   // not beyond 0.6
    CHECK(stick_dpad({0.5f, 0.5f}, kNone) == kNone);
    // Pressed: kept down to 0.4, released below it.
    CHECK(stick_dpad({0.0f, 0.45f}, kUp) == kUp);
    CHECK(stick_dpad({0.0f, 0.4f}, kUp) == kUp);
    CHECK(stick_dpad({0.0f, 0.35f}, kUp) == kNone);
    // A held direction stays while its axis holds, even if the other grows.
    CHECK(stick_dpad({0.9f, 0.5f}, kUp) == kUp);
    // Released below 0.4, the larger axis beyond 0.6 presses at once.
    CHECK(stick_dpad({0.9f, 0.3f}, kUp) == kRight);
}

TEST_CASE("d-pad emulation: off writes nothing; the stick presses only where it is free (22 §7)") {
    DpadEmulation emu;
    const auto t0 = std::chrono::steady_clock::time_point{} + 10s;
    Controls c;
    c.move = {0.0f, 0.9f};

    DpadEmulationResult r = emu.update(c, true, false, t0);
    CHECK(r.dpad == kNone);
    CHECK_FALSE(r.stick_used);

    r = emu.update(c, false, true, t0); // walking: the stick walks
    CHECK(r.dpad == kNone);
    CHECK_FALSE(r.stick_used);

    r = emu.update(c, true, true, t0); // a menu or an interaction
    CHECK(r.dpad == kUp);
    CHECK(r.stick_used);

    c.move = {0.0f, 0.2f};
    r      = emu.update(c, true, true, t0);
    CHECK(r.dpad == kNone);
    CHECK_FALSE(r.stick_used);
}

TEST_CASE("d-pad emulation: the modifier is the left stick click held 0.3 s (22 §7)") {
    DpadEmulation emu;
    auto t = std::chrono::steady_clock::time_point{} + 10s;
    Controls c;
    c.sprint = true; // the left stick click
    c.move   = {0.9f, 0.0f};

    DpadEmulationResult r = emu.update(c, false, true, t);
    CHECK_FALSE(r.modifier_held); // a click alone is sprint
    CHECK(r.dpad == kNone);
    r = emu.update(c, false, true, t + 299ms);
    CHECK_FALSE(r.modifier_held);
    r = emu.update(c, false, true, t + 300ms);
    CHECK(r.modifier_held);
    CHECK(r.dpad == kRight);
    CHECK(r.stick_used);

    // Released: the hold starts again.
    c.sprint = false;
    r        = emu.update(c, false, true, t + 400ms);
    CHECK_FALSE(r.modifier_held);
    CHECK(r.dpad == kNone);
    c.sprint = true;
    r        = emu.update(c, false, true, t + 500ms);
    CHECK_FALSE(r.modifier_held);
}

TEST_CASE("d-pad emulation on wands: the left pad clicked on an edge (22 §7)") {
    DpadEmulation emu;
    const auto t = std::chrono::steady_clock::time_point{} + 10s;
    Controls c;
    c.touchpad_scheme = true;
    c.sprint          = true; // the left pad click
    c.move            = {0.1f, -0.5f};

    DpadEmulationResult r = emu.update(c, false, true, t);
    CHECK(r.modifier_held); // at once, no hold
    CHECK(r.dpad == kDown);

    c.move = {0.1f, 0.1f}; // the centre stays sprint
    r      = emu.update(c, false, true, t);
    CHECK_FALSE(r.modifier_held);
    CHECK(r.dpad == kNone);
}

TEST_CASE("the pad drives every map 0 and 1 action on a pressed d-pad direction (22 §7)") {
    DpadPad p;
    CHECK_FALSE(p.down(0, DpadPad::kPrevMainWeapon));

    p.pad.set_dpad(kLeft);
    CHECK(p.down(0, DpadPad::kPrevMainWeapon));
    CHECK_FALSE(p.down(0, DpadPad::kNextMainWeapon));
    CHECK(p.down(1, DpadPad::kMenuLeft));
    CHECK_FALSE(p.down(1, DpadPad::kMenuUp));
    CHECK_FALSE(p.down(0, 0)); // stock A

    p.pad.set_dpad(Dpad{.up = true, .right = true}); // a real d-pad may press two
    CHECK(p.down(1, DpadPad::kMenuUp));
    CHECK(p.down(1, DpadPad::kMenuRight));
    CHECK(p.down(0, DpadPad::kNextMainWeapon));
    CHECK_FALSE(p.down(1, DpadPad::kMenuDown));
    CHECK_FALSE(p.down(2, 0));                                         // map 2 gets no VR state
    CHECK_FALSE(p.pad.merge_down(game::kTierBase[1] + 19 + 2, false)); // an analog entry

    p.pad.set_dpad(kNone);
    CHECK_FALSE(p.down(1, DpadPad::kMenuUp));
    CHECK(p.pad.merge_down(game::kTierBase[1] + DpadPad::kMenuUp, true)); // the real pad still counts
}

TEST_CASE("the control mapper hands the d-pad to the pad and frees the stick (22 §7)") {
    DpadPad p;
    const ActionTier& menu = p.pad.tiers()[game::kMenuMap];
    namespace ma           = game::menu_action;

    Controls c;
    c.move       = {0.0f, 0.9f};
    p.stick.free = true; // a menu on the virtual screen

    // Key off: v1, the stick moves the menu's analog and presses no d-pad.
    p.mapper.write(c, false);
    CHECK(menu.analog[ma::kMoveY] == doctest::Approx(0.9f));
    CHECK_FALSE(p.down(1, DpadPad::kMenuUp));

    // Key on: the d-pad press, and the stick's movement value is 0.
    p.controls.dpad_emulation = true;
    p.mapper.write(c, false);
    CHECK(p.down(1, DpadPad::kMenuUp));
    CHECK(menu.analog[ma::kMoveY] == 0.0f);

    // A controller's own d-pad presses whatever the key.
    p.controls.dpad_emulation = false;
    Controls own;
    own.dpad = kDown;
    (void)p.pad.on_activity(false, PlayerDevices::NoManager); // clear earlier activity
    p.mapper.write(own, false);
    CHECK(p.down(1, DpadPad::kMenuDown));
    CHECK(p.pad.on_activity(false, PlayerDevices::NoManager).any); // VR activity (04 §5.1)
    p.mapper.write(Controls{}, false);
    CHECK_FALSE(p.down(1, DpadPad::kMenuDown));
}

TEST_CASE("the control mapper: the held modifier is not sprint, its stick does not walk (22 §7)") {
    DpadPad p;
    p.controls.dpad_emulation = true;
    const ActionTier& player  = p.pad.tiers()[game::kPlayerMap];
    namespace pa              = game::player_action;

    Controls c;
    c.sprint = true;
    c.move   = {-0.9f, 0.0f};
    p.mapper.write(c, false);
    CHECK(player.digital[pa::kSprint] == 1); // not held long enough yet
    CHECK(player.analog[pa::kMoveX] == doctest::Approx(-0.9f));
    p.clock.advance(300ms);
    p.mapper.write(c, false);
    CHECK(player.digital[pa::kSprint] == 0);
    CHECK(player.analog[pa::kMoveX] == 0.0f);
    CHECK(p.down(0, DpadPad::kPrevMainWeapon));
}

TEST_CASE("the thumbstick scheme passes a controller's d-pad on") {
    HandInput left, right;
    left.dpad.left   = true;
    right.dpad.up    = true;
    const Controls c = thumbstick_controls(left, right);
    CHECK(c.dpad == Dpad{.up = true, .left = true});
    CHECK(touchpad_controls(HandInput{}, HandInput{}).dpad == kNone);
}

TEST_CASE("the control mapper logs each change of the d-pad with Dev. Debug Options (22 §7)") {
    DpadPad p;
    p.log.debug = true;

    Controls c;
    c.dpad = kUp;
    p.mapper.write(c, false);
    p.mapper.write(c, false); // no change: no line
    p.mapper.write(Controls{}, false);
    CHECK(p.log.lines.size() == 2);
    CHECK(p.log.contains("[input] d-pad up true down false left false right false (emulated false)"));
}
