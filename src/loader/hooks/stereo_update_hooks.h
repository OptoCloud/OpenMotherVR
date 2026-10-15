#pragma once

#include "game/layout.h"

// The game-side hooks around the stereo device: the per-frame stereo
// update, placement of the menu screen and the HUD panel, the stereo mode
// stack and the left-stick prompt check. Every function here replaces a
// game function at a call site and keeps that function's ABI (noted per
// function). The hook installer does the patching.
//
// From: FUN_1001f8a0, FUN_1001fc20, FUN_1001ffd0, FUN_100201e0,
// FUN_10020220, FUN_10003100. Analysis: dumps/06c_detours_and_patches.md.
namespace omvr::loader::hooks {

// Stereo::Update(float dt). __thiscall, ret 4.
int __fastcall stereo_update(game::StereoManager* self, void* edx, float dt) noexcept;

// Matrix44* BuildUiQuad(Matrix44* out, float size, float inv_aspect, float dist).
// __cdecl; the caller copies the result through the returned pointer.
float* __cdecl place_virtual_screen(float* out, float size, float inv_aspect, float dist) noexcept;
float* __cdecl place_hud_panel(float* out, float size, float inv_aspect, float dist) noexcept;

// Stereo::PushMode(int) (__thiscall, ret 4) and Stereo::PopMode() (ret 0).
void __fastcall push_mode(game::StereoManager* self, void* edx, int mode) noexcept;
void __fastcall pop_mode(game::StereoManager* self, void* edx) noexcept;

// IsMoveOnLeftStick() for the control prompts. __thiscall, no args.
bool __fastcall is_move_on_left_stick(void* self, void* edx) noexcept;

} // namespace omvr::loader::hooks
