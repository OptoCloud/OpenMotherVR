// OpenMotherVR-Provenance: clean
#include "game/hands/haptic_sites.h"

#include "game/addresses.h"
#include "game/code_address.h"
#include "game/detour.h"
#include "game/hands/hand_sites.h"

#include <array>
#include <cstdint>
#include <string_view>

namespace omvr::game::hands {

namespace {

namespace ports = core::diag::ports;

constexpr std::string_view kLogNoAddress =
    "[hook] haptics: off; this build has no addresses for their game sources (21 §20)";
constexpr std::string_view kLogRumbleCode =
    "[hook] haptics, game rumble: off; unexpected code at RumbleSetMotors (RVA {:08X}) or its vtable "
    "slot (RVA {:08X})";
constexpr std::string_view kLogRumbleOn =
    "[hook] haptics, game rumble: in place (RumbleSetMotors RVA {:08X})";
constexpr std::string_view kLogPlayerOn = "[hook] haptics, damage and death: in place (the player's health)";
constexpr std::string_view kLogFiringCode =
    "[hook] haptics, firing: off; unexpected code at CurrentItem (RVA {:08X})";
constexpr std::string_view kLogFiringOn = "[hook] haptics, firing: in place (CurrentItem RVA {:08X})";
constexpr std::string_view kLogTrackerOff =
    "[hook] haptics, tracker: off; the tracker scan's call is not in place";

// 21 §2.9: RumbleSetMotors' first instruction, `movss xmm0, [esp+4]` (the
// low-frequency level).
constexpr std::array<std::uint8_t, 6> kRumbleSetMotorsStart = {0xF3, 0x0F, 0x10, 0x44, 0x24, 0x04};

// 21 §2.9: the rumble object's vtable holds RumbleSetMotors in slot 0, so
// the function is the one the game calls for every pad rumble.
bool rumble_expected(const GameImage& g, const HapticRvas& r) noexcept {
    return holds_address(g.at_rva(r.rumble_vtable), g.at_rva(r.rumble_set_motors)) &&
           x86::matches(g.at_rva(r.rumble_set_motors), kRumbleSetMotorsStart);
}

// A detour of the function's entry rather than the vtable slot, so a call
// that does not go through the vtable is seen too.
bool install_rumble(const GameImage& g, Patcher& p, const HapticRvas& r, const HapticHooks& hooks,
                    HapticTargets& out) noexcept {
    if (!rumble_expected(g, r)) {
        ports::logf(p.log(), kLogRumbleCode, r.rumble_set_motors, r.rumble_vtable);
        return false;
    }
    if (!detour(g.at_rva(r.rumble_set_motors), hooks.rumble_set_motors, kRumbleSetMotorsStart,
                out.rumble_set_motors, "RumbleSetMotors", p.log())) {
        return false;
    }
    ports::logf(p.log(), kLogRumbleOn, r.rumble_set_motors);
    return true;
}

bool install_firing(const GameImage& g, Patcher& p, HandTargets& hand_out) noexcept {
    const HandRvas& r = kHandRvas[g.version];
    if (!find_current_item(g, hand_out)) {
        ports::logf(p.log(), kLogFiringCode, r.current_item);
        return false;
    }
    ports::logf(p.log(), kLogFiringOn, r.current_item);
    return true;
}

} // namespace

HapticSitesOutcome install_haptic_sites(const GameImage& g, Patcher& p, const HapticHooks& hooks,
                                        HandTargets& hand_out, HapticTargets& out) noexcept {
    const HapticRvas& r = kHapticRvas[g.version];
    if (r.rumble_set_motors == 0 || kHandRvas[g.version].character_registry == 0) {
        p.log().line(kLogNoAddress);
        return {};
    }
    HapticSitesOutcome o;
    o.rumble = install_rumble(g, p, r, hooks, out);
    o.player = find_player(g, p.log(), hand_out);
    if (o.player) {
        p.log().line(kLogPlayerOn);
        o.firing = install_firing(g, p, hand_out);
    }
    o.tracker = install_tracker_call(g, p, hooks.tracker_scan, hand_out);
    if (!o.tracker) p.log().line(kLogTrackerOff);
    return o;
}

} // namespace omvr::game::hands
