// OpenMotherVR-Provenance: clean
#include "game/hands/action_sites.h"

#include "game/addresses.h"
#include "game/code_address.h"
#include "game/detour.h"
#include "game/x86.h"

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

// 21 §2.11: the first bytes of each start method. The melee kill start,
// the play request and its counterpart open with an exception frame whose
// `push imm32` holds their handler's VA: those four bytes are 0 here and
// filled in from the table for this image (see with_handler), since the
// loader rebases them.
//
// The SEH frame's pieces: `push -1` (the frame's state), `push <handler>`
// and `mov eax, fs:[0]` (the thread's handler chain).
constexpr std::uint8_t kSehStateNone                = 0xFF; // -1 as a sign-extended imm8
constexpr std::array<std::uint8_t, 2> kPushSehState = {x86::kPushImm8, kSehStateNone};
constexpr std::array<std::uint8_t, 5> kPushHandler  = {x86::kPushImm32, 0, 0, 0, 0};
constexpr std::array<std::uint8_t, 6> kLoadSehChain = {x86::kFsPrefix, x86::kMovEaxMoffs, 0, 0, 0, 0};
constexpr auto kSubEsp54                            = x86::sub_esp_imm8(0x54);
constexpr std::array<std::uint8_t, 2> kMovEsiEcx    = {x86::kMovR32Rm32,
                                                       x86::modrm_reg(x86::Reg::Esi, x86::Reg::Ecx)};

// `push ebp; mov ebp, esp; and esp, -16; sub esp, 0xB4`.
constexpr auto kMeleeStart = x86::concat(x86::kAlignedFramePrologue, x86::sub_esp_imm32(0xB4));
static_assert(kMeleeStart == std::array<std::uint8_t, 12>{0x55, 0x8B, 0xEC, 0x83, 0xE4, 0xF0, 0x81, 0xEC,
                                                          0xB4, 0x00, 0x00, 0x00});
// `push -1; push <handler>; mov eax, fs:[0]` (its first five bytes).
constexpr auto kMeleeKillStart = x86::concat(x86::concat(kPushSehState, kPushHandler),
                                             std::array<std::uint8_t, 5>{x86::kFsPrefix, x86::kMovEaxMoffs});
static_assert(kMeleeKillStart == std::array<std::uint8_t, 12>{0x6A, 0xFF, 0x68, 0x00, 0x00, 0x00, 0x00, 0x64,
                                                              0xA1, 0x00, 0x00, 0x00});
constexpr std::size_t kMeleeKillHandlerAt = kPushSehState.size() + x86::kImm32Offset;
// `sub esp, 0x54; push ebx; push ebp; push esi; mov esi, ecx; mov ecx,
// [esi+4]; push edi`.
constexpr auto kTrapBreakStart = x86::concat(
    x86::concat(
        x86::concat(kSubEsp54, std::array<std::uint8_t, 3>{x86::kPushEbx, x86::kPushEbp, x86::kPushEsi}),
        kMovEsiEcx),
    std::array<std::uint8_t, 4>{x86::kMovR32Rm32, x86::modrm_disp8(x86::Reg::Ecx, x86::Reg::Esi), 0x04,
                                x86::kPushEdi});
static_assert(kTrapBreakStart == std::array<std::uint8_t, 12>{0x83, 0xEC, 0x54, 0x53, 0x55, 0x56, 0x8B, 0xF1,
                                                              0x8B, 0x4E, 0x04, 0x57});
// `sub esp, 0x54; push ebx; push esi; push edi; sub esp, 0x10; mov
// [esp+...], esp` (up to its SIB byte).
constexpr auto kMeleeInterceptStart = x86::concat(
    x86::concat(
        x86::concat(kSubEsp54, std::array<std::uint8_t, 3>{x86::kPushEbx, x86::kPushEsi, x86::kPushEdi}),
        x86::sub_esp_imm8(0x10)),
    std::array<std::uint8_t, 3>{x86::kMovRm32R32, x86::modrm_disp8(x86::Reg::Esp, x86::Reg::Esp),
                                x86::kSibEsp});
static_assert(kMeleeInterceptStart == std::array<std::uint8_t, 12>{0x83, 0xEC, 0x54, 0x53, 0x56, 0x57, 0x83,
                                                                   0xEC, 0x10, 0x89, 0x64, 0x24});
// `push ebp; mov ebp, esp; and esp, -16; sub esp, 0x64; push ebx; push
// esi; mov ...` (the move's opcode).
constexpr auto kImpactStandingStart =
    x86::concat(x86::concat(x86::kAlignedFramePrologue, x86::sub_esp_imm8(0x64)),
                std::array<std::uint8_t, 3>{x86::kPushEbx, x86::kPushEsi, x86::kMovR32Rm32});
static_assert(kImpactStandingStart == std::array<std::uint8_t, 12>{0x55, 0x8B, 0xEC, 0x83, 0xE4, 0xF0, 0x83,
                                                                   0xEC, 0x64, 0x53, 0x56, 0x8B});
// `push ebp; mov ebp, esp; and esp, -16; sub esp, 0xD4`.
constexpr auto kTrapVictimStart = x86::concat(x86::kAlignedFramePrologue, x86::sub_esp_imm32(0xD4));
static_assert(kTrapVictimStart == std::array<std::uint8_t, 12>{0x55, 0x8B, 0xEC, 0x83, 0xE4, 0xF0, 0x81, 0xEC,
                                                               0xD4, 0x00, 0x00, 0x00});
// 21 §2.11: the play request and its counterpart, up to the end of the
// handler's `push imm32`: `mov eax, fs:[0]; push -1; push <handler>` and
// `push -1; mov eax, fs:[0]; push <handler>`.
constexpr auto kPlayRequestStart = x86::concat(x86::concat(kLoadSehChain, kPushSehState), kPushHandler);
static_assert(kPlayRequestStart == std::array<std::uint8_t, 13>{0x64, 0xA1, 0x00, 0x00, 0x00, 0x00, 0x6A,
                                                                0xFF, 0x68, 0x00, 0x00, 0x00, 0x00});
constexpr auto kPlayCounterpartStart = x86::concat(x86::concat(kPushSehState, kLoadSehChain), kPushHandler);
static_assert(kPlayCounterpartStart == std::array<std::uint8_t, 13>{0x6A, 0xFF, 0x64, 0xA1, 0x00, 0x00, 0x00,
                                                                    0x00, 0x68, 0x00, 0x00, 0x00, 0x00});
constexpr std::size_t kPlayHandlerAt = kLoadSehChain.size() + kPushSehState.size() + x86::kImm32Offset;
static_assert(kMeleeKillStart[kMeleeKillHandlerAt - 1] == x86::kPushImm32 &&
              kPlayRequestStart[kPlayHandlerAt - 1] == x86::kPushImm32 &&
              kPlayCounterpartStart[kPlayHandlerAt - 1] == x86::kPushImm32);

// `start` with the handler this image's `push imm32` should hold: 0 (no
// match) when the build has no handler address.
template <std::size_t At, std::size_t N>
std::array<std::uint8_t, N> with_handler(const GameImage& g, const std::array<std::uint8_t, N>& start,
                                         std::uint32_t handler_rva) noexcept {
    return x86::with_imm32<At>(start, handler_rva != 0 ? address_of(g.at_rva(handler_rva)) : 0);
}
// 21 §2.7: SetBodyVelocities, `push ebp; mov ebp, esp; and esp, -16; sub
// esp, 0x3C`.
constexpr auto kSetBodyVelocitiesStart = x86::concat(x86::kAlignedFramePrologue, x86::sub_esp_imm8(0x3C));
static_assert(kSetBodyVelocitiesStart ==
              std::array<std::uint8_t, 9>{0x55, 0x8B, 0xEC, 0x83, 0xE4, 0xF0, 0x83, 0xEC, 0x3C});

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
    return s.rva != 0 && s.hook && x86::matches(g.at_rva(s.rva), s.start);
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
    const auto melee_kill = with_handler<kMeleeKillHandlerAt>(g, kMeleeKillStart, r.melee_kill_eh_handler);
    const std::array<Site<ActionStartFn>, 5> sites{{
        {r.melee_kill_start, melee_kill, h.melee_kill, &out.melee_kill, "ACT_PLAYER_MELEE_KILL start"},
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
    const auto request = with_handler<kPlayHandlerAt>(g, kPlayRequestStart, r.play_request_eh_handler);
    const auto counterpart =
        with_handler<kPlayHandlerAt>(g, kPlayCounterpartStart, r.play_counterpart_eh_handler);
    const std::array<Site<PlayAnimationFn>, 2> sites{{
        {r.play_animation_request, request, h.play_request, &out.play_request, "the play request"},
        {r.play_animation_counterpart, counterpart, h.play_counterpart, &out.play_counterpart,
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
