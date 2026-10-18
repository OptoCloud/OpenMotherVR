// OpenMotherVR-Provenance: clean
#pragma once

#include "game/game_image.h"
#include "game/patcher.h"

// Body visibility's game sites (spec 21 §2.3, §10, §20): the two calls in
// FinalPoseStep that hand the player's model its pose. Checked and pointed
// at the mod only once Body.Visible is not Full (21 §1: at the defaults
// nothing is hooked).
namespace omvr::game::hands {

// void __thiscall SkinSetPose(render object, const bone transforms*, int
// count), ret 8.
using SkinSetPoseFn = void(__fastcall*)(void* self, void* edx, const void* pose, int count);

// What the hook calls: the game's own SkinSetPose.
struct BodyTargets {
    SkinSetPoseFn skin_set_pose = nullptr;
};

// Checks FinalPoseStep, its thunk and the job that runs it, SkinSetPose and
// both its calls before writing either (12 §4); on a mismatch, or on a
// build without the addresses, nothing is written and that is logged.
// `out` is set before the first redirect. True when both calls go to
// `hook` (SkinSetPose's ABI).
bool install_body_sites(const GameImage& g, Patcher& p, const void* hook, BodyTargets& out) noexcept;

} // namespace omvr::game::hands
