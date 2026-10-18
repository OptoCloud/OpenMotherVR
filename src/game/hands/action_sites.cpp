// OpenMotherVR-Provenance: clean
#include "game/hands/action_sites.h"

#include "game/addresses.h"
#include "game/code_address.h"
#include "game/detour.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string_view>

namespace omvr::game::hands {

namespace {

namespace ports = core::diag::ports;

constexpr std::string_view kLogNoAddress =
    "[hook] hands, player actions: off; this build has no addresses for them (21 §20)";
constexpr std::string_view kLogCode = "[hook] hands, {}: off; unexpected code at {} (RVA {:08X})";
constexpr std::string_view kLogOn   = "[hook] hands, {}: in place ({} sites)";

// 21 §2.11: the first bytes of each start method.
constexpr std::array<std::uint8_t, 12> kMeleeStart          = {0x55, 0x8B, 0xEC, 0x83, 0xE4, 0xF0,
                                                               0x81, 0xEC, 0xB4, 0x00, 0x00, 0x00};
constexpr std::array<std::uint8_t, 12> kMeleeKillStart      = {0x6A, 0xFF, 0x68, 0x78, 0x6A, 0x12,
                                                               0x01, 0x64, 0xA1, 0x00, 0x00, 0x00};
constexpr std::array<std::uint8_t, 12> kTrapBreakStart      = {0x83, 0xEC, 0x54, 0x53, 0x55, 0x56,
                                                               0x8B, 0xF1, 0x8B, 0x4E, 0x04, 0x57};
constexpr std::array<std::uint8_t, 12> kMeleeInterceptStart = {0x83, 0xEC, 0x54, 0x53, 0x56, 0x57,
                                                               0x83, 0xEC, 0x10, 0x89, 0x64, 0x24};
constexpr std::array<std::uint8_t, 12> kImpactStandingStart = {0x55, 0x8B, 0xEC, 0x83, 0xE4, 0xF0,
                                                               0x83, 0xEC, 0x64, 0x53, 0x56, 0x8B};
constexpr std::array<std::uint8_t, 12> kTrapVictimStart     = {0x55, 0x8B, 0xEC, 0x83, 0xE4, 0xF0,
                                                               0x81, 0xEC, 0xD4, 0x00, 0x00, 0x00};
// 21 §2.11: the play request and its counterpart.
constexpr std::array<std::uint8_t, 12> kPlayRequestStart     = {0x64, 0xA1, 0x00, 0x00, 0x00, 0x00,
                                                                0x6A, 0xFF, 0x68, 0xE0, 0x26, 0x12};
constexpr std::array<std::uint8_t, 12> kPlayCounterpartStart = {0x6A, 0xFF, 0x64, 0xA1, 0x00, 0x00,
                                                                0x00, 0x00, 0x68, 0xA8, 0x10, 0x12};
// 21 §2.7: SetBodyVelocities, `push ebp; mov ebp, esp; and esp, -16; sub
// esp, 0x3C`.
constexpr std::array<std::uint8_t, 9> kSetBodyVelocitiesStart = {0x55, 0x8B, 0xEC, 0x83, 0xE4,
                                                                 0xF0, 0x83, 0xEC, 0x3C};

// 21 §2.11: the melee starts are slot 24 of their vtables; the play
// request and its counterpart slots +0x3D0 and +0x3D4 of their table.
constexpr std::size_t kMeleeStartSlot            = 24;
constexpr std::size_t kPlayRequestSlotOffset     = 0x3D0;
constexpr std::size_t kPlayCounterpartSlotOffset = 0x3D4;
constexpr std::size_t kSlotSize                  = sizeof(std::uint32_t);

// One detour: where, the expected bytes, the replacement, the trampoline.
template <class Fn>
struct Site {
    std::uint32_t rva;
    std::span<const std::uint8_t> start;
    Fn hook;
    Fn* trampoline;
    const char* name;
};

template <class Fn>
bool site_expected(const GameImage& g, const Site<Fn>& s) noexcept {
    return s.rva != 0 && s.hook && std::ranges::equal(s.start, std::span{g.at_rva(s.rva), s.start.size()});
}

// Verifies every site, then detours them in order. A detour that fails
// after others went in leaves those in place: each calls the game's own
// function, so the game runs as before.
template <class Fn, std::size_t N>
bool install_group(const GameImage& g, Patcher& p, const std::array<Site<Fn>, N>& sites, bool slots_ok,
                   const char* feature) noexcept {
    for (const auto& s : sites) {
        if (!site_expected(g, s)) {
            ports::logf(p.log(), kLogCode, feature, s.name, s.rva);
            return false;
        }
    }
    if (!slots_ok) {
        ports::logf(p.log(), kLogCode, feature, "its method table", sites[0].rva);
        return false;
    }
    for (const auto& s : sites) {
        if (!detour(g.at_rva(s.rva), s.hook, s.start, *s.trampoline, s.name, p.log())) return false;
    }
    ports::logf(p.log(), kLogOn, feature, N);
    return true;
}

bool slot_holds(const GameImage& g, std::uint32_t table, std::size_t offset, std::uint32_t fn) noexcept {
    return table != 0 && fn != 0 && holds_address(g.at_rva(table) + offset, g.at_rva(fn));
}

bool install_melee(const GameImage& g, Patcher& p, const ActionRvas& r, const ActionSiteFns& h,
                   ActionSiteFns& out) noexcept {
    const std::array<Site<ActionStartFn>, 2> sites{{
        {r.melee_start_a, kMeleeStart, h.melee_a, &out.melee_a, "ACT_PLAYER_MELEE start (first)"},
        {r.melee_start_b, kMeleeStart, h.melee_b, &out.melee_b, "ACT_PLAYER_MELEE start (second)"},
    }};
    const bool slots = slot_holds(g, r.melee_vtable_a, kMeleeStartSlot * kSlotSize, r.melee_start_a) &&
                       slot_holds(g, r.melee_vtable_b, kMeleeStartSlot * kSlotSize, r.melee_start_b);
    return install_group(g, p, sites, slots, "melee arm hand-off");
}

bool install_actions(const GameImage& g, Patcher& p, const ActionRvas& r, const ActionSiteFns& h,
                     ActionSiteFns& out) noexcept {
    const std::array<Site<ActionStartFn>, 5> sites{{
        {r.melee_kill_start, kMeleeKillStart, h.melee_kill, &out.melee_kill, "ACT_PLAYER_MELEE_KILL start"},
        {r.trap_break_start, kTrapBreakStart, h.trap_break, &out.trap_break, "ACT_PLAYER_TRAP_BREAK start"},
        {r.melee_intercept_start, kMeleeInterceptStart, h.melee_intercept, &out.melee_intercept,
         "ACT_PLAYER_MELEE_INTERCEPT start"},
        {r.impact_standing_start, kImpactStandingStart, h.impact_standing, &out.impact_standing,
         "ACT_PLAYER_IMPACT_STANDING start"},
        {r.trap_victim_start, kTrapVictimStart, h.trap_victim, &out.trap_victim,
         "ACT_PLAYER_TRAP_VICTIM start"},
    }};
    return install_group(g, p, sites, true, "player action episodes");
}

bool install_play_animation(const GameImage& g, Patcher& p, const ActionRvas& r, const ActionSiteFns& h,
                            ActionSiteFns& out) noexcept {
    const std::array<Site<PlayAnimationFn>, 2> sites{{
        {r.play_animation_request, kPlayRequestStart, h.play_request, &out.play_request, "the play request"},
        {r.play_animation_counterpart, kPlayCounterpartStart, h.play_counterpart, &out.play_counterpart,
         "the play request's counterpart"},
    }};
    const bool slots =
        slot_holds(g, r.play_animation_table, kPlayRequestSlotOffset, r.play_animation_request) &&
        slot_holds(g, r.play_animation_table, kPlayCounterpartSlotOffset, r.play_animation_counterpart);
    return install_group(g, p, sites, slots, "play-animation log");
}

bool install_velocities(const GameImage& g, Patcher& p, const ActionRvas& r, const ActionSiteFns& h,
                        ActionSiteFns& out) noexcept {
    const std::array<Site<SetBodyVelocitiesFn>, 1> sites{{
        {r.set_body_velocities, kSetBodyVelocitiesStart, h.set_body_velocities, &out.set_body_velocities,
         "SetBodyVelocities"},
    }};
    return install_group(g, p, sites, true, "throw launch log");
}

} // namespace

ActionSitesOutcome install_action_sites(const GameImage& g, Patcher& p, const ActionSiteFns& hooks,
                                        const ActionSitesWanted& wanted, ActionSiteFns& out) noexcept {
    const ActionRvas& r = kActionRvas[g.version];
    if (r.melee_start_a == 0) {
        p.log().line(kLogNoAddress);
        return {};
    }
    ActionSitesOutcome o;
    if (wanted.starts) {
        o.melee   = install_melee(g, p, r, hooks, out);
        o.actions = install_actions(g, p, r, hooks, out);
    }
    if (wanted.play_animation) o.play_animation = install_play_animation(g, p, r, hooks, out);
    if (wanted.velocities) o.velocities = install_velocities(g, p, r, hooks, out);
    return o;
}

} // namespace omvr::game::hands
