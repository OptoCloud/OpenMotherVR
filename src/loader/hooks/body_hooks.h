// OpenMotherVR-Provenance: clean
#pragma once

#include "core/hands/body_hider.h"
#include "core/hands/body_visibility.h"
#include "game/game_image.h"
#include "game/hands/body_objects.h"
#include "game/layout.h"

#include <cstdint>
#include <optional>

// The body features' hooks (spec 21 §10 to §12): body visibility in the
// pose the player's model is drawn from, real crouch and the control
// hand-back, each frame from the stereo update.
namespace omvr::loader::hooks {

// At attach (full patch set): remembers the game image for a later install
// and, when Body.Visible is already not Full, installs the body visibility
// sites now. At the defaults nothing is checked or written (21 §1).
void prepare_body(const game::GameImage& g) noexcept;

// The skeleton at `skeleton` read into `read`: its bones (views into
// `read`), or why it could not be read. Game thread.
struct SkeletonBonesRead {
    std::optional<core::hands::SkeletonBones> bones;
    core::hands::SkeletonReadFault fault;
};
SkeletonBonesRead read_skeleton_bones(std::uintptr_t skeleton, game::hands::SkeletonRead& read) noexcept;

// FinalPoseStep's SkinSetPose calls are pointed at skin_set_pose, installed
// now if they were not tried yet (the arm pose edit runs there, 21 §4.2).
// Game thread, after attach.
bool body_pose_sites() noexcept;

// Once per frame from the stereo update while the device runs. Game thread,
// in the CAMERA_UPDATE stage: before the frame's CHARACTER_IK_AND_RAGDOLL
// stage, whose final-pose jobs read what this publishes (21 §17).
void body_on_stereo_update(game::StereoInterface& device) noexcept;

// Replaces FinalPoseStep's two calls of SkinSetPose (21 §2.3): __thiscall
// on a render object, (const bone transforms*, int count), ret 8. Edits
// the player's arms (21 §4.2), then hides what Body.Visible hides (21
// §10.2). Job threads.
void __fastcall skin_set_pose(void* object, void* edx, const void* pose, int count) noexcept;

} // namespace omvr::loader::hooks
