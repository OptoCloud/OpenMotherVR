// OpenMotherVR-Provenance: clean
#pragma once

#include "core/vr/ports/vr_runtime.h"
#include "game/game_image.h"
#include "game/layout.h"
#include "util/math/matrix.h"

// The tracked hands' hooks (spec 21 §3 to §6): the hand poses taken with
// each head pose, the aim ray after the game's AimRayUpdate, and the motion
// tracker's facing around the game's TrackerScan.
namespace omvr::loader::hooks {

// At attach (full patch set): remembers the game image for a later install
// and, when Hands.Enabled is already on, installs the hand sites now. With
// it off nothing is checked or written (21 §1).
void prepare_hands(const game::GameImage& g) noexcept;

// The stereo device handed the game `head` (slot 11): takes the hand poses
// of the same prediction and the frame's activity signals, and updates the
// hands. Installs the hand sites the first time Hands.Enabled is on.
// Game thread.
void hands_on_head_pose(game::StereoInterface& si, core::vr::ports::VrRuntime& runtime,
                        const util::Matrix34& head) noexcept;

// Replaces the player's call of AimRayUpdate (21 §2.5). __thiscall on the
// weapon component, no args. Game thread.
void __fastcall aim_ray_update(void* component, void* edx) noexcept;

// Replaces the tracker's call of TrackerScan (21 §2.6). __thiscall on the
// tracker, no args, bool in AL.
bool __fastcall tracker_scan(void* tracker, void* edx) noexcept;

} // namespace omvr::loader::hooks
