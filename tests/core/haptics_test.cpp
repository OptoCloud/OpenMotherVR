// OpenMotherVR-Provenance: clean
// core/hands haptics (spec 21 §14): each event's pulse, hands and
// strength from the table of §14.2, the game's rumble and the focus and
// lost-hand rules of §14.3. Expected values are the spec's numbers.

#include "core/config/settings.h"
#include "core/hands/hand_aim.h"
#include "core/hands/haptics.h"
#include "fakes/fakes.h"

#include <doctest/doctest.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <optional>

namespace {

using namespace omvr::core::hands;
using omvr::core::input::Interaction;
using omvr::core::vr::ports::HapticFrame;
using omvr::game::WeaponType;
using namespace std::chrono_literals;

constexpr float kEps = 1e-4f;
constexpr std::array<bool, 2> kBoth{true, true};

struct Rig {
    omvr::core::config::HapticsSettings settings{};
    omvr::tests::FakeClock clock;
    omvr::tests::RecordingLog log;
    Haptics haptics{settings, clock, log};

    HapticFrame frame(bool focus = true, std::array<bool, 2> tracked = kBoth) {
        return haptics.next(focus, tracked);
    }
    TimePoint now() const { return clock.now(); }
};

Health health(std::uint16_t current, std::uint16_t maximum = 100, std::uintptr_t player = 1) {
    return {player, current, maximum};
}

FireSample fire(bool pressed, WeaponType w, bool gate = true) { return {pressed, w, gate}; }

void check_pulse(const omvr::core::vr::ports::HapticHand& h, float amplitude, float seconds) {
    CHECK(h.play);
    CHECK(h.amplitude == doctest::Approx(amplitude).epsilon(kEps));
    CHECK(h.seconds == doctest::Approx(seconds).epsilon(kEps));
    CHECK(h.level == doctest::Approx(amplitude).epsilon(kEps));
}

void check_silent(const omvr::core::vr::ports::HapticHand& h) {
    CHECK_FALSE(h.play);
    CHECK(h.level == 0.0f);
}

} // namespace

TEST_CASE("haptics: damage amplitude is 0.3 + 2 * lost fraction, capped at 1") {
    CHECK(damage_amplitude(100, 90, 100) == doctest::Approx(0.5f));
    CHECK(damage_amplitude(100, 95, 100) == doctest::Approx(0.4f));
    CHECK(damage_amplitude(100, 60, 100) == doctest::Approx(1.0f));
    CHECK(damage_amplitude(50, 50, 100) == 0.0f);
    CHECK(damage_amplitude(50, 40, 0) == 1.0f);
}

TEST_CASE("haptics: ranged weapons are types 1, 2, 3 and 5") {
    const auto ranged = [](WeaponType t) { return is_ranged(weapon_class(t)); };
    CHECK(ranged(WeaponType::Flamethrower));
    CHECK(ranged(WeaponType::Pistol));
    CHECK(ranged(WeaponType::Shotgun));
    CHECK(ranged(WeaponType::Boltgun));
    CHECK_FALSE(ranged(WeaponType::NoWeapon));
    CHECK_FALSE(ranged(WeaponType::Melee));
    CHECK_FALSE(ranged(WeaponType::Cattleprod));
    CHECK_FALSE(ranged(WeaponType::Unknown));
}

TEST_CASE("haptics: damage pulses both hands for 150 ms, death 1.0 for 600 ms") {
    Rig r;
    r.haptics.on_health(health(100));
    check_silent(r.frame()[0]);
    r.haptics.on_health(health(90));
    auto f = r.frame();
    check_pulse(f[0], 0.5f, 0.15f);
    check_pulse(f[1], 0.5f, 0.15f);

    r.clock.advance(200ms);
    r.haptics.on_health(health(0));
    f = r.frame();
    check_pulse(f[0], 1.0f, 0.6f);
    check_pulse(f[1], 1.0f, 0.6f);
}

TEST_CASE("haptics: no damage pulse on healing, a new player or with Damage off") {
    Rig r;
    r.haptics.on_health(health(50));
    r.haptics.on_health(health(80)); // a medikit
    check_silent(r.frame()[0]);
    r.haptics.on_health(health(40, 100, 2)); // a reload: another character
    check_silent(r.frame()[0]);
    r.haptics.on_health(std::nullopt); // no player
    r.haptics.on_health(health(20, 100, 2));
    check_silent(r.frame()[0]);
    r.settings.damage = false;
    r.haptics.on_health(health(10, 100, 2));
    check_silent(r.frame()[0]);
}

TEST_CASE("haptics: a pulse is played once, for its duration, and Strength scales it") {
    Rig r;
    r.settings.strength = 0.5f;
    r.haptics.on_health(health(100));
    r.haptics.on_health(health(90));
    check_pulse(r.frame()[1], 0.25f, 0.15f);
    r.clock.advance(50ms);
    const auto f = r.frame();
    CHECK_FALSE(f[1].play);                      // started once
    CHECK(f[1].level == doctest::Approx(0.25f)); // still in effect (legacy pulses it)
    r.clock.advance(100ms);
    CHECK(r.frame()[1].level == 0.0f); // over after 150 ms
}

TEST_CASE("haptics: a tracker scan ticks the left hand only with a contact in range while raised") {
    Rig r;
    r.haptics.on_tracker_scan(false, true); // raised, nothing on it
    check_silent(r.frame()[0]);
    r.haptics.on_tracker_scan(true, false); // a contact, but not raised
    check_silent(r.frame()[0]);
    r.haptics.on_tracker_scan(true, true);
    const auto f = r.frame();
    check_pulse(f[0], 0.15f, 0.02f);
    check_silent(f[1]);
    r.clock.advance(30ms); // the 20 ms tick is over
    r.settings.tracker = false;
    r.haptics.on_tracker_scan(true, true);
    check_silent(r.frame()[0]);
}

TEST_CASE("haptics: firing pulses the right hand on the press, with a ranged weapon and the gate open") {
    Rig r;
    r.haptics.on_fire(fire(true, WeaponType::Pistol), r.now());
    auto f = r.frame();
    check_pulse(f[1], 0.6f, 0.06f);
    check_silent(f[0]);

    r.clock.advance(100ms);
    r.haptics.on_fire(fire(true, WeaponType::Pistol), r.now()); // still held: one shot
    check_silent(r.frame()[1]);
    r.haptics.on_fire(fire(false, WeaponType::Pistol), r.now());
    r.haptics.on_fire(fire(true, WeaponType::Pistol, false), r.now()); // gate closed
    check_silent(r.frame()[1]);
    r.haptics.on_fire(fire(false, WeaponType::Melee), r.now());
    r.haptics.on_fire(fire(true, WeaponType::Melee), r.now()); // not ranged
    check_silent(r.frame()[1]);
    r.haptics.on_fire({false, std::nullopt, true}, r.now());
    r.haptics.on_fire({true, std::nullopt, true}, r.now()); // no weapon
    check_silent(r.frame()[1]);
    r.settings.firing = false;
    r.haptics.on_fire(fire(false, WeaponType::Shotgun), r.now());
    r.haptics.on_fire(fire(true, WeaponType::Shotgun), r.now());
    check_silent(r.frame()[1]);
}

TEST_CASE("haptics: each press of a ranged weapon is a shot: press, release, press pulses twice") {
    Rig r;
    r.haptics.on_fire(fire(true, WeaponType::Pistol), r.now());
    check_pulse(r.frame()[1], 0.6f, 0.06f);
    r.clock.advance(100ms);
    r.haptics.on_fire(fire(false, WeaponType::Pistol), r.now());
    check_silent(r.frame()[1]);
    r.clock.advance(100ms);
    r.haptics.on_fire(fire(true, WeaponType::Pistol), r.now());
    check_pulse(r.frame()[1], 0.6f, 0.06f);
    r.clock.advance(100ms);
    // A release seen while the gate is closed still ends the press.
    r.haptics.on_fire(fire(false, WeaponType::Pistol, false), r.now());
    r.haptics.on_fire(fire(true, WeaponType::Pistol), r.now());
    check_pulse(r.frame()[1], 0.6f, 0.06f);
}

TEST_CASE("haptics: the flamethrower pulses 0.3 again as each pulse ends while held") {
    Rig r;
    r.haptics.on_fire(fire(true, WeaponType::Flamethrower), r.now());
    check_pulse(r.frame()[1], 0.3f, 0.06f);
    r.clock.advance(30ms);
    r.haptics.on_fire(fire(true, WeaponType::Flamethrower), r.now());
    CHECK_FALSE(r.frame()[1].play);
    r.clock.advance(30ms);
    r.haptics.on_fire(fire(true, WeaponType::Flamethrower), r.now());
    check_pulse(r.frame()[1], 0.3f, 0.06f);
    r.clock.advance(70ms);
    r.haptics.on_fire(fire(false, WeaponType::Flamethrower), r.now());
    check_silent(r.frame()[1]);
}

TEST_CASE("haptics: hacking pulses both hands at its start and at its end") {
    Rig r;
    r.haptics.on_interaction(Interaction::Hacking);
    auto f = r.frame();
    check_pulse(f[0], 0.4f, 0.08f);
    check_pulse(f[1], 0.4f, 0.08f);
    r.clock.advance(100ms);
    r.haptics.on_interaction(Interaction::Hacking); // unchanged
    check_silent(r.frame()[0]);
    r.haptics.on_interaction(Interaction::None);
    check_pulse(r.frame()[0], 0.4f, 0.08f);
    r.clock.advance(100ms);
    r.settings.hacking = false;
    r.haptics.on_interaction(Interaction::Hacking);
    check_silent(r.frame()[0]);
}

TEST_CASE("haptics: cutting pulses as hacking does; other interactions do not") {
    Rig r;
    r.haptics.on_interaction(Interaction::Keypad);
    check_silent(r.frame()[0]);
    r.haptics.on_interaction(Interaction::Cutting);
    check_pulse(r.frame()[1], 0.4f, 0.08f);
    r.clock.advance(100ms);
    r.haptics.on_interaction(Interaction::Hacking); // still hacking or cutting
    check_silent(r.frame()[1]);
    r.haptics.on_interaction(Interaction::Generic); // the cut ended back to generic
    check_pulse(r.frame()[1], 0.4f, 0.08f);
}

TEST_CASE("haptics: the game's pad curve gives each motor's speed (21 §2.9)") {
    // At or below 0.05 both motors stop.
    CHECK(pad_motor_speed(0.0f, PadMotor::Low) == 0.0f);
    CHECK(pad_motor_speed(0.05f, PadMotor::Low) == 0.0f);
    CHECK(pad_motor_speed(0.05f, PadMotor::High) == 0.0f);
    CHECK(pad_motor_speed(std::nanf(""), PadMotor::High) == 0.0f);
    // Low motor: 11468.626 L + 5242.8 up to 0.4, 75365.25 L - 9830.25 above.
    CHECK(pad_motor_speed(0.08f, PadMotor::Low) ==
          doctest::Approx((11468.626f * 0.08f + 5242.8f) / 65535.0f));
    CHECK(pad_motor_speed(0.4f, PadMotor::Low) == doctest::Approx((11468.626f * 0.4f + 5242.8f) / 65535.0f));
    CHECK(pad_motor_speed(0.5f, PadMotor::Low) == doctest::Approx((75365.25f * 0.5f - 9830.25f) / 65535.0f));
    // High motor: 57343 L + 8192.
    CHECK(pad_motor_speed(0.08f, PadMotor::High) == doctest::Approx((57343.0f * 0.08f + 8192.0f) / 65535.0f));
    CHECK(pad_motor_speed(0.5f, PadMotor::High) == doctest::Approx((57343.0f * 0.5f + 8192.0f) / 65535.0f));
    // 1.0 runs both at full speed; nothing above it.
    CHECK(pad_motor_speed(1.0f, PadMotor::Low) == doctest::Approx(1.0f));
    CHECK(pad_motor_speed(1.0f, PadMotor::High) == doctest::Approx(1.0f));
    CHECK(pad_motor_speed(2.0f, PadMotor::High) == doctest::Approx(1.0f));
    // The spec's examples: a level of 0.08 runs the motors at about 9 % and
    // 20 %.
    CHECK(pad_motor_speed(0.08f, PadMotor::Low) == doctest::Approx(0.094f).epsilon(0.01));
    CHECK(pad_motor_speed(0.08f, PadMotor::High) == doctest::Approx(0.195f).epsilon(0.01));
}

TEST_CASE("haptics: a rumble level plays at the motor speed squared, nothing below 0.05 (21 §14.4)") {
    CHECK(rumble_amplitude(1.0f, PadMotor::Low) == doctest::Approx(1.0f));
    CHECK(rumble_amplitude(1.0f, PadMotor::High) == doctest::Approx(1.0f));
    // The tracker's level 0.08: about 0.01 and 0.04, both under the floor.
    CHECK(rumble_amplitude(0.08f, PadMotor::Low) == 0.0f);
    CHECK(rumble_amplitude(0.08f, PadMotor::High) == 0.0f);
    const float low  = (75365.25f * 0.5f - 9830.25f) / 65535.0f;
    const float high = (57343.0f * 0.5f + 8192.0f) / 65535.0f;
    CHECK(rumble_amplitude(0.5f, PadMotor::Low) == doctest::Approx(low * low));
    CHECK(rumble_amplitude(0.5f, PadMotor::High) == doctest::Approx(high * high));
}

TEST_CASE("haptics: the game's rumble is renewed each frame, low motor left, high motor right") {
    Rig r;
    r.haptics.on_rumble(1.0f, 0.5f, r.now());
    const float high = rumble_amplitude(0.5f, PadMotor::High);
    auto f           = r.frame();
    check_pulse(f[0], 1.0f, kRumbleRenewal.count());
    check_pulse(f[1], high, kRumbleRenewal.count());
    r.clock.advance(11ms);
    check_pulse(r.frame()[0], 1.0f, kRumbleRenewal.count());

    // A quiet level (the raised tracker's 0.08) plays nothing: what was
    // playing is stopped.
    r.haptics.on_rumble(0.08f, 0.08f, r.now());
    f = r.frame();
    CHECK_FALSE(f[0].play);
    CHECK_FALSE(f[1].play);
    CHECK(f[0].stop);
    CHECK(f[1].stop);
    CHECK_FALSE(r.frame()[0].stop); // once
}

TEST_CASE("haptics: a rumble level is held until the next call; Strength applies after the floor") {
    Rig r;
    r.settings.strength = 0.1f;
    r.haptics.on_rumble(1.0f, 1.0f, r.now()); // sent once, as the game does for a steady level
    r.clock.advance(300ms);
    // 0.1 is below the floor, but the floor is on the amplitude before
    // Strength: the rumble is felt, weaker.
    check_pulse(r.frame()[0], 0.1f, kRumbleRenewal.count());
    r.haptics.on_rumble(0.0f, 0.0f, r.now());
    CHECK(r.frame()[0].stop);
}

TEST_CASE("haptics: a rumble level not set again for 0.5 s ends; GameRumble off plays none") {
    Rig r;
    r.haptics.on_rumble(0.6f, 0.6f, r.now());
    CHECK(r.frame()[0].play);
    r.clock.advance(400ms);
    CHECK(r.frame()[0].play);
    r.clock.advance(200ms);
    CHECK(r.frame()[0].stop);

    r.haptics.on_rumble(0.6f, 0.6f, r.now());
    r.settings.game_rumble = false;
    check_silent(r.frame()[0]);
}

TEST_CASE("haptics: rumble and a pulse together play the larger; the pulse resumes when rumble ends") {
    Rig r;
    r.haptics.on_rumble(0.5f, 0.5f, r.now()); // 0.18 and 0.32: weaker than the pulse
    r.haptics.on_interaction(Interaction::Hacking);
    check_pulse(r.frame()[0], 0.4f, kRumbleRenewal.count());
    r.clock.advance(20ms);
    r.haptics.on_rumble(0.0f, 0.0f, r.now());
    const auto f = r.frame();
    check_pulse(f[0], 0.4f, 0.06f); // what is left of the 80 ms pulse
    CHECK_FALSE(f[0].stop);
}

TEST_CASE("haptics: nothing without input focus or for a lost hand, and nothing replayed after") {
    Rig r;
    r.haptics.on_rumble(0.5f, 0.5f, r.now());
    CHECK(r.frame()[0].play);
    r.haptics.on_interaction(Interaction::Hacking);
    auto f = r.frame(false);
    check_silent(f[0]);
    check_silent(f[1]);
    CHECK(f[0].stop); // the rumble's vibration ends
    CHECK(f[1].stop);

    r.haptics.on_rumble(0.0f, 0.0f, r.now());
    r.clock.advance(10ms);
    f = r.frame();
    check_silent(f[0]); // the hacking pulse came without focus: not played late
    CHECK_FALSE(f[0].stop);

    r.haptics.on_interaction(Interaction::None);
    f = r.frame(true, {false, true});
    check_silent(f[0]);
    check_pulse(f[1], 0.4f, 0.08f);
}

TEST_CASE("haptics: the debug window gives the highest levels, the calls and the highest played") {
    Rig r;
    r.haptics.log_rumble_window();
    CHECK(r.log.lines.empty()); // no level was set: no line

    r.haptics.on_rumble(0.08f, 0.08f, r.now());
    r.haptics.on_rumble(1.0f, 0.3f, r.now());
    r.haptics.on_rumble(0.2f, 0.6f, r.now());
    r.settings.strength = 0.5f;
    (void)r.frame(); // plays half the amplitude of the last pair
    const float played =
        std::max(rumble_amplitude(0.2f, PadMotor::Low), rumble_amplitude(0.6f, PadMotor::High)) * 0.5f;
    const RumbleWindow w = r.haptics.take_rumble_window();
    CHECK(w.calls == 3);
    CHECK(w.highest_low == doctest::Approx(1.0f));
    CHECK(w.highest_high == doctest::Approx(0.6f));
    CHECK(w.highest_played == doctest::Approx(played));

    // The held level starts the next window, so a steady rumble keeps
    // being reported; once dropped, nothing is logged.
    r.haptics.log_rumble_window();
    CHECK(r.log.contains("game rumble since the last line: highest level low 0.200 high 0.600, 0 calls"));
    r.clock.advance(600ms);
    r.haptics.log_rumble_window(); // the window still began with 0.2 / 0.6
    r.log.lines.clear();
    r.haptics.log_rumble_window();
    CHECK(r.log.lines.empty());
}
