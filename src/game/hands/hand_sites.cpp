// OpenMotherVR-Provenance: clean
#include "game/hands/hand_sites.h"

#include "game/addresses.h"
#include "game/code_address.h"
#include "game/x86.h"

#include <array>
#include <cstdint>
#include <cstring>
#include <string_view>

namespace omvr::game::hands {

namespace {

namespace ports = core::diag::ports;

constexpr std::string_view kLogNoAddress = "[hook] hands: off; this build has no addresses for them (21 §20)";
constexpr std::string_view kLogRegistry =
    "[hook] hands: off; PlayerGetter (RVA {:08X}) does not read CharacterRegistry (RVA {:08X})";
constexpr std::string_view kLogAimCode =
    "[hook] hands, aim from hand: off; unexpected code at AimRayUpdate's call (RVA {:08X}), AimRayUpdate, "
    "CurrentItem or the fire alignment test (RVA {:08X})";
constexpr std::string_view kLogAimOn =
    "[hook] hands, aim from hand: in place (AimRayUpdate call RVA {:08X}, fire alignment test RVA {:08X})";
constexpr std::string_view kLogTrackerCode =
    "[hook] hands, motion tracker in hand: off; unexpected code at TrackerScan's call (RVA {:08X}) or "
    "TrackerScan";
constexpr std::string_view kLogTrackerOn =
    "[hook] hands, motion tracker in hand: in place (TrackerScan call RVA {:08X})";

// 21 §2.2: PlayerGetter starts `mov eax, [CharacterRegistry]; mov eax,
// [eax+8]`.
constexpr std::array<std::uint8_t, 3> kLoadCharacterList = {x86::kMovR32Rm32, 0x40, 0x08};
constexpr std::size_t kGetterListOffset                  = x86::kImm32Offset + x86::kRel32Size;
// 21 §2.5: AimRayUpdate's prologue, `push ebp; mov ebp, esp; and esp, -16;
// sub esp, 0x54`.
constexpr std::uint8_t kAimRayFrameSize = 0x54;
constexpr auto kAimRayUpdatePrologue =
    x86::concat(x86::kAlignedFramePrologue,
                std::array<std::uint8_t, 3>{x86::kGroup1Imm8Signed, x86::kModRmSubEsp, kAimRayFrameSize});
static_assert(kAimRayUpdatePrologue ==
              std::array<std::uint8_t, 9>{0x55, 0x8B, 0xEC, 0x83, 0xE4, 0xF0, 0x83, 0xEC, 0x54});
// 21 §2.5: CurrentItem's first instruction, `cmp byte [ecx+0x1510], 0`.
constexpr std::array<std::uint8_t, 7> kCurrentItemStart = {0x80, 0xB9, 0x10, 0x15, 0x00, 0x00, 0x00};
// 21 §2.5: the fire alignment test, `comiss xmm0, [threshold]` (the
// operand is the threshold's address) followed by `jbe +0x11`.
constexpr std::array<std::uint8_t, 3> kComissXmm0Abs = {x86::kTwoByte, x86::kComiss, x86::kModAbsolute};
constexpr std::array<std::uint8_t, 2> kJbeShort11    = {x86::kJbeRel8, 0x11};
constexpr std::size_t kGateJbeOffset                 = x86::kComissDispOffset + x86::kRel32Size;
// 21 §2.6: TrackerScan's prologue, `push ebp; mov ebp, esp; and esp, -16`.
constexpr auto kTrackerScanPrologue = x86::kAlignedFramePrologue;

bool registry_confirmed(const GameImage& g, const HandRvas& r) noexcept {
    const std::uint8_t* getter = g.at_rva(r.player_getter);
    return getter[0] == x86::kMovEaxMoffs &&
           holds_address(getter + x86::kImm32Offset, g.at_rva(r.character_registry)) &&
           x86::matches(getter + kGetterListOffset, kLoadCharacterList);
}

bool fire_gate_expected(const GameImage& g, const HandRvas& r) noexcept {
    const std::uint8_t* site = g.at_rva(r.fire_gate_compare);
    return x86::matches(site, kComissXmm0Abs) &&
           holds_address(site + x86::kComissDispOffset, g.at_rva(r.fire_gate_threshold)) &&
           x86::matches(site + kGateJbeOffset, kJbeShort11);
}

bool aim_code_expected(const GameImage& g, const HandRvas& r) noexcept {
    return calls(g.at_rva(r.aim_ray_call), g.at_rva(r.aim_ray_update)) &&
           x86::matches(g.at_rva(r.aim_ray_update), kAimRayUpdatePrologue) &&
           x86::matches(g.at_rva(r.current_item), kCurrentItemStart) && fire_gate_expected(g, r);
}

// 21 §5: the aim ray call and the fire alignment test, both or neither.
bool install_aim(const GameImage& g, Patcher& p, const HandRvas& r, const HandHooks& hooks,
                 HandTargets& out) noexcept {
    if (!aim_code_expected(g, r)) {
        ports::logf(p.log(), kLogAimCode, r.aim_ray_call, r.fire_gate_compare);
        return false;
    }
    std::memcpy(hooks.fire_gate_threshold, g.at_rva(r.fire_gate_threshold), sizeof(float));
    out.aim_ray_update = g.at_rva(r.aim_ray_update);
    out.current_item   = g.at_rva(r.current_item);
    if (!p.write_pointer(g.at_rva(r.fire_gate_compare) + x86::kComissDispOffset, hooks.fire_gate_threshold) ||
        !p.redirect_call(g.at_rva(r.aim_ray_call), hooks.aim_ray_update, "AimRayUpdate")) {
        return false;
    }
    ports::logf(p.log(), kLogAimOn, r.aim_ray_call, r.fire_gate_compare);
    return true;
}

} // namespace

// 21 §6, §14: the tracker scan's call.
bool install_tracker_call(const GameImage& g, Patcher& p, const void* hook, HandTargets& out) noexcept {
    const HandRvas& r = kHandRvas[g.version];
    if (r.tracker_scan_call == 0) return false;
    // Already pointed at the hook by the other feature that needs it.
    if (out.tracker_scan && calls(g.at_rva(r.tracker_scan_call), static_cast<const std::uint8_t*>(hook)))
        return true;
    if (!calls(g.at_rva(r.tracker_scan_call), g.at_rva(r.tracker_scan)) ||
        !x86::matches(g.at_rva(r.tracker_scan), kTrackerScanPrologue)) {
        ports::logf(p.log(), kLogTrackerCode, r.tracker_scan_call);
        return false;
    }
    out.tracker_scan = g.at_rva(r.tracker_scan);
    if (!p.redirect_call(g.at_rva(r.tracker_scan_call), hook, "TrackerScan")) return false;
    ports::logf(p.log(), kLogTrackerOn, r.tracker_scan_call);
    return true;
}

bool find_player(const GameImage& g, core::diag::ports::Log& log, HandTargets& out) noexcept {
    const HandRvas& r = kHandRvas[g.version];
    if (r.character_registry == 0) return false;
    if (!registry_confirmed(g, r)) {
        ports::logf(log, kLogRegistry, r.player_getter, r.character_registry);
        return false;
    }
    out.registry = g.at_rva(r.character_registry);
    return true;
}

bool find_current_item(const GameImage& g, HandTargets& out) noexcept {
    const HandRvas& r = kHandRvas[g.version];
    if (r.current_item == 0 || !x86::matches(g.at_rva(r.current_item), kCurrentItemStart)) return false;
    out.current_item = g.at_rva(r.current_item);
    return true;
}

HandSitesOutcome install_hand_sites(const GameImage& g, Patcher& p, const HandHooks& hooks,
                                    HandTargets& out) noexcept {
    const HandRvas& r = kHandRvas[g.version];
    if (r.character_registry == 0) {
        p.log().line(kLogNoAddress);
        return {};
    }
    if (!find_player(g, p.log(), out)) return {};
    HandSitesOutcome o;
    o.aim     = install_aim(g, p, r, hooks, out);
    o.tracker = install_tracker_call(g, p, hooks.tracker_scan, out);
    return o;
}

} // namespace omvr::game::hands
