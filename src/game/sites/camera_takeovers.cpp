// OpenMotherVR-Provenance: clean
#include "game/sites/camera_takeovers.h"

#include "game/addresses.h"
#include "game/code_address.h"
#include "game/sites/area_log.h"
#include "game/sites/site_check.h"
#include "game/x86.h"

#include <array>
#include <cstdint>
#include <string_view>

namespace omvr::game::sites {

namespace {

constexpr std::string_view kArea = "camera takeovers";

// 23 §2.4: FollowSteer's first instructions, `push esi; mov esi, ecx; mov
// eax, [esi+0x34]; movss ...`.
constexpr std::array<std::uint8_t, 8> kFollowSteerStart = {0x56, 0x8B, 0xF1, 0x8B, 0x46, 0x34, 0xF3, 0x0F};
// The re-align's first instructions, read in the 2015 AI.exe: `mov eax,
// [ecx+0x34]; test eax, eax; je; mov eax, ...`.
constexpr std::array<std::uint8_t, 8> kFollowRealignStart = {0x8B, 0x41, 0x34, 0x85, 0xC0, 0x74, 0x31, 0x8B};
// 23 §2.4: the StealCamera start's first instructions, `sub esp, 0x40;
// push esi; push edi; mov edi, [esp+...]`.
constexpr std::array<std::uint8_t, 8> kStealCameraStart = {0x83, 0xEC, 0x40, 0x56, 0x57, 0x8B, 0x7C, 0x24};
// 23 §2.4: CameraResetRequest's first instructions, `push ebp; mov ebp,
// esp; and esp, -64; sub esp, ...`.
constexpr auto kResetRequestStart =
    x86::concat(x86::kFramePrologue,
                std::array<std::uint8_t, 5>{x86::kGroup1Imm8Signed, x86::kModRmAndEsp, x86::kAlign64Imm8,
                                            x86::kGroup1Imm8Signed, x86::kModRmSubEsp});
static_assert(kResetRequestStart ==
              std::array<std::uint8_t, 8>{0x55, 0x8B, 0xEC, 0x83, 0xE4, 0xC0, 0x83, 0xEC});

bool expected(const Check& c, const CameraTakeoverRvas& r) noexcept {
    return c.code(r.follow_steer, "FollowSteer", kFollowSteerStart) &&
           c.code(r.follow_realign, "the follow camera's re-align", kFollowRealignStart) &&
           c.code(r.steal_camera_start, "the StealCamera start", kStealCameraStart) &&
           c.entry(r.steal_camera_entry, r.steal_camera_start, "the StealCamera start's entry") &&
           c.call(r.steal_camera_steer_call, r.follow_steer, "the StealCamera steer") &&
           c.entry(r.level_camera_stop_entry, r.level_camera_stop, "the CameraResource stop's entry") &&
           c.call(r.level_camera_stop_steer_call, r.follow_steer, "the CameraResource stop's steer") &&
           c.call(r.level_camera_stop_realign_call, r.follow_realign, "the CameraResource stop's re-align") &&
           c.code(r.reset_request, "CameraResetRequest", kResetRequestStart) &&
           c.call(r.reset_request_lookup_call, r.follow_lookup, "CameraResetRequest's lookup") &&
           c.call(r.reset_request_realign_call, r.follow_realign, "CameraResetRequest's re-align") &&
           c.call(r.state_entry_lookup_call, r.follow_lookup, "the state entry's lookup") &&
           c.call(r.state_entry_realign_call, r.follow_realign, "the state entry's re-align");
}

} // namespace

bool install_camera_takeover_sites(const GameImage& g, Patcher& p, const CameraTakeoverHooks& hooks,
                                   CameraTakeoverTargets& out) noexcept {
    const CameraTakeoverRvas& r = kCameraTakeoverRvas[g.version];
    if (!known_or_off(p.log(), r.follow_steer, kArea)) return false;
    if (!expected(Check{g, p, kArea}, r)) return false;
    out.steer   = CodeAddress{g.at_rva(r.follow_steer)};
    out.realign = CodeAddress{g.at_rva(r.follow_realign)};
    const std::array calls{
        CallRedirect{g.at_rva(r.steal_camera_steer_call), code_pointer(hooks.steal_camera_steer),
                     "StealCamera steer"},
        CallRedirect{g.at_rva(r.level_camera_stop_steer_call), code_pointer(hooks.level_camera_exit_steer),
                     "CameraResource stop steer"},
        CallRedirect{g.at_rva(r.level_camera_stop_realign_call),
                     code_pointer(hooks.level_camera_exit_realign), "CameraResource stop re-align"},
        CallRedirect{g.at_rva(r.reset_request_realign_call), code_pointer(hooks.reset_realign),
                     "CameraResetRequest re-align"},
        CallRedirect{g.at_rva(r.state_entry_realign_call), code_pointer(hooks.state_entry_realign),
                     "state entry re-align"}};
    static_assert(calls.size() == 5);
    // A failed redirect leaves the earlier ones pointing at replacements
    // that, with the group off, always run the game's function.
    if (!redirect_all(p, calls)) return false;
    area_on(p.log(), kArea, static_cast<int>(calls.size()));
    return true;
}

} // namespace omvr::game::sites
