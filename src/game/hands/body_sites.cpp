// OpenMotherVR-Provenance: clean
#include "game/hands/body_sites.h"

#include "game/addresses.h"
#include "game/code_address.h"
#include "game/hands/body_objects.h"
#include "game/x86.h"

#include <array>
#include <cstdint>
#include <string_view>

namespace omvr::game::hands {

namespace {

namespace ports = core::diag::ports;

constexpr std::string_view kLogNoAddress =
    "[hook] body visibility: off; this build has no addresses for it (21 §20)";
constexpr std::string_view kLogCode =
    "[hook] body visibility: off; unexpected code at FinalPoseStep (RVA {:08X}), its thunk or job, "
    "SkinSetPose (RVA {:08X}) or its calls (RVA {:08X}, {:08X})";
constexpr std::string_view kLogOn = "[hook] body visibility: in place (SkinSetPose calls RVA {:08X}, {:08X})";

// 21 §2.3: FinalPoseStep's prologue, `mov eax, fs:[0]; push -1` (an SEH
// frame).
constexpr std::array<std::uint8_t, 8> kFinalPoseStepStart = {0x64, 0xA1, 0x00, 0x00, 0x00, 0x00, 0x6A, 0xFF};
// 21 §2.3: the thunk, `mov ecx, [esp+4]; add ecx, 0x3F0`, then `jmp
// FinalPoseStep`: the character's animation component.
constexpr std::array<std::uint8_t, 10> kThunkStart = {0x8B, 0x4C, 0x24, 0x04, 0x81,
                                                      0xC1, 0xF0, 0x03, 0x00, 0x00};
constexpr std::size_t kThunkJumpOffset             = kThunkStart.size();
static_assert(kCharacterAnimation == 0x3F0, "the thunk's add is the animation component's offset");
// 21 §2.3: SkinSetPose's prologue, `push ebp; mov ebp, esp; and esp, -16;
// sub esp, 0x154`.
constexpr std::array<std::uint8_t, 12> kSkinSetPoseStart = {0x55, 0x8B, 0xEC, 0x83, 0xE4, 0xF0,
                                                            0x81, 0xEC, 0x54, 0x01, 0x00, 0x00};

// `jmp rel32` at `site` to `target`.
bool jumps(const std::uint8_t* site, const std::uint8_t* target) noexcept {
    return *site == x86::kJmpRel32 &&
           address_of(site + x86::kJmpRel32Size) + read_u32(site + x86::kCallOperandOffset) ==
               address_of(target);
}

bool code_expected(const GameImage& g, const BodyRvas& r) noexcept {
    const std::uint8_t* thunk = g.at_rva(r.final_pose_thunk);
    const std::uint8_t* push  = g.at_rva(r.final_pose_job_push);
    return x86::matches(g.at_rva(r.final_pose_step), kFinalPoseStepStart) &&
           x86::matches(thunk, kThunkStart) && jumps(thunk + kThunkJumpOffset, g.at_rva(r.final_pose_step)) &&
           *push == x86::kPushImm32 && read_u32(push + x86::kImm32Offset) == address_of(thunk) &&
           x86::matches(g.at_rva(r.skin_set_pose), kSkinSetPoseStart) &&
           calls(g.at_rva(r.skin_call_single), g.at_rva(r.skin_set_pose)) &&
           calls(g.at_rva(r.skin_call_parts), g.at_rva(r.skin_set_pose));
}

} // namespace

bool install_body_sites(const GameImage& g, Patcher& p, const void* hook, BodyTargets& out) noexcept {
    const BodyRvas& r = body_rvas(g.version);
    if (r.final_pose_step == 0) {
        p.log().line(kLogNoAddress);
        return false;
    }
    if (!code_expected(g, r)) {
        ports::logf(p.log(), kLogCode, r.final_pose_step, r.skin_set_pose, r.skin_call_single,
                    r.skin_call_parts);
        return false;
    }
    out.skin_set_pose = CodeAddress{g.at_rva(r.skin_set_pose)};
    if (!p.redirect_call(g.at_rva(r.skin_call_single), hook, "SkinSetPose") ||
        !p.redirect_call(g.at_rva(r.skin_call_parts), hook, "SkinSetPose")) {
        return false;
    }
    ports::logf(p.log(), kLogOn, r.skin_call_single, r.skin_call_parts);
    return true;
}

} // namespace omvr::game::hands
