// OpenMotherVR-Provenance: clean
#pragma once

#include "game/game_image.h"
#include "game/hands/arm_objects.h"
#include "game/patcher.h"

// The arm pose edit's game sites (spec 21 §2.1, §2.3, §4.2, §20), checked and
// pointed at the mod only when Hands.Enabled is on (21 §1). The edit itself
// runs in body visibility's SkinSetPose calls (body_sites.h), which the
// loader installs for it.
namespace omvr::game::hands {

// The replacements the sites are pointed at, each with the ABI of the
// function its call reaches.
struct ArmHooks {
    const void* final_pose_pass; // cdecl (character list, count)
    const void* attach_query;    // __fastcall (component, edx, bone, selector, out): BoneWorldTransform's ABI
    const void* limb_ik;         // stdcall (four args)
};

// Which parts went in.
struct ArmSitesOutcome {
    bool pass      = false; // the final-pose pass call: the arm edit is placed each frame (21 §4.2)
    bool held_item = false; // the attachment query: the held item follows the edit (21 §4.2 item 6)
    bool limb_ik   = false; // the limb IK calls (21 §4.2 item 7)
};

// Checks every site of each part before writing any of it (12 §4); a part
// whose code differs, or whose addresses this build lacks, stays off and is
// logged. The held item and the limb IK need the pass. `out` is set before
// the first redirect.
ArmSitesOutcome install_arm_sites(const GameImage& g, Patcher& p, const ArmHooks& hooks,
                                  ArmTargets& out) noexcept;

} // namespace omvr::game::hands
