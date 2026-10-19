// OpenMotherVR-Provenance: clean
#include "game/hands/arm_sites.h"

#include "game/addresses.h"
#include "game/code_address.h"
#include "game/x86.h"

#include <array>
#include <cstdint>
#include <string_view>

namespace omvr::game::hands {

namespace {

namespace ports = core::diag::ports;

constexpr std::string_view kLogNoAddress =
    "[hook] hands, arms: off; this build has no addresses for the arm pose edit (21 §20)";
constexpr std::string_view kLogPassCode =
    "[hook] hands, arms: off; unexpected code at the final-pose pass call (RVA {:08X}) or the equipment "
    "update "
    "call (RVA {:08X})";
constexpr std::string_view kLogPassOn = "[hook] hands, arms: in place (final-pose pass call RVA {:08X})";
constexpr std::string_view kLogItemCode =
    "[hook] hands, arms: the held item keeps the game's placement; unexpected code at BoneWorldTransform "
    "(RVA "
    "{:08X}) or the attachment query (RVA {:08X}, selector push RVA {:08X})";
constexpr std::string_view kLogItemOn =
    "[hook] hands, arms: the held item follows the edit (attachment query RVA {:08X})";
constexpr std::string_view kLogIkCode =
    "[hook] hands, arms: the game's limb IK keeps running; unexpected code at it (RVA {:08X}) or its calls";
constexpr std::string_view kLogIkOn =
    "[hook] hands, arms: the game's limb IK is left out for the player's first-person skeleton while the "
    "hands are tracked (calls RVA {:08X}, {:08X}, {:08X}, {:08X})";

// 21 §2.3: BoneWorldTransform's prologue, `push ebp; mov ebp, esp; and esp,
// -16; sub esp, 0x54`.
constexpr std::uint8_t kBoneWorldFrameSize = 0x54;
constexpr auto kBoneWorldTransformStart =
    x86::concat(x86::kAlignedFramePrologue,
                std::array<std::uint8_t, 3>{x86::kGroup1Imm8Signed, x86::kModRmSubEsp, kBoneWorldFrameSize});
static_assert(kBoneWorldTransformStart ==
              std::array<std::uint8_t, 9>{0x55, 0x8B, 0xEC, 0x83, 0xE4, 0xF0, 0x83, 0xEC, 0x54});
// 21 §20: the attachment query's selector, `push 6`.
constexpr std::array<std::uint8_t, x86::kPushImm8Size> kPushAttachSelector = {
    x86::kPushImm8, static_cast<std::uint8_t>(kAttachPoseSelector)};
static_assert(kPushAttachSelector == std::array<std::uint8_t, 2>{0x6A, 0x06});
// 21 §2.3: the limb IK's prologue, `push ebp; mov ebp, esp; and esp, -16`.
constexpr auto kLimbIkStart = x86::kAlignedFramePrologue;

bool pass_expected(const GameImage& g, const ArmRvas& r) noexcept {
    return calls(g.at_rva(r.final_pose_pass_call), g.at_rva(r.final_pose_pass)) &&
           calls(g.at_rva(r.equipment_update_call), g.at_rva(r.equipment_update));
}

bool item_expected(const GameImage& g, const ArmRvas& r) noexcept {
    return x86::matches(g.at_rva(r.bone_world_transform), kBoneWorldTransformStart) &&
           calls(g.at_rva(r.attach_query_call), g.at_rva(r.bone_world_transform)) &&
           x86::matches(g.at_rva(r.attach_selector_push), kPushAttachSelector);
}

std::array<std::uint32_t, 4> limb_ik_calls(const ArmRvas& r) noexcept {
    return {r.limb_ik_call_a, r.limb_ik_call_b, r.limb_ik_call_c, r.limb_ik_call_d};
}

bool ik_expected(const GameImage& g, const ArmRvas& r) noexcept {
    if (!x86::matches(g.at_rva(r.limb_ik), kLimbIkStart)) return false;
    for (const std::uint32_t call : limb_ik_calls(r))
        if (!calls(g.at_rva(call), g.at_rva(r.limb_ik))) return false;
    return true;
}

bool install_ik(const GameImage& g, Patcher& p, const ArmRvas& r, const void* hook) noexcept {
    const auto sites = limb_ik_calls(r);
    std::array<CallRedirect, sites.size()> redirects{};
    for (std::size_t i = 0; i < sites.size(); ++i) redirects[i] = {g.at_rva(sites[i]), hook, "limb IK"};
    if (!redirect_all(p, redirects)) return false;
    ports::logf(p.log(), kLogIkOn, sites[0], sites[1], sites[2], sites[3]);
    return true;
}

} // namespace

ArmSitesOutcome install_arm_sites(const GameImage& g, Patcher& p, const ArmHooks& hooks,
                                  ArmTargets& out) noexcept {
    const ArmRvas& r = kArmRvas[g.version];
    if (r.final_pose_pass == 0) {
        p.log().line(kLogNoAddress);
        return {};
    }
    if (!pass_expected(g, r)) {
        ports::logf(p.log(), kLogPassCode, r.final_pose_pass_call, r.equipment_update_call);
        return {};
    }
    // Every part is checked before the first write.
    const bool item = item_expected(g, r);
    const bool ik   = ik_expected(g, r);
    if (!item)
        ports::logf(p.log(), kLogItemCode, r.bone_world_transform, r.attach_query_call,
                    r.attach_selector_push);
    if (!ik) ports::logf(p.log(), kLogIkCode, r.limb_ik);

    out.final_pose_pass  = g.at_rva(r.final_pose_pass);
    out.equipment_update = g.at_rva(r.equipment_update);
    if (item) out.bone_world_transform = g.at_rva(r.bone_world_transform);
    if (ik) out.limb_ik = g.at_rva(r.limb_ik);

    ArmSitesOutcome o;
    if (!p.redirect_call(g.at_rva(r.final_pose_pass_call), hooks.final_pose_pass, "final-pose pass"))
        return o;
    o.pass = true;
    ports::logf(p.log(), kLogPassOn, r.final_pose_pass_call);
    if (item && p.redirect_call(g.at_rva(r.attach_query_call), hooks.attach_query, "attachment query")) {
        o.held_item = true;
        ports::logf(p.log(), kLogItemOn, r.attach_query_call);
    }
    if (ik) o.limb_ik = install_ik(g, p, r, hooks.limb_ik);
    return o;
}

} // namespace omvr::game::hands
